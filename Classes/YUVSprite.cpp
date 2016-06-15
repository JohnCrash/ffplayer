/*
 * 用来在cocos2dx-x 2.x.x，中直接显示yuv图像
 */
#include "cocos2d.h"
#include "YUVSprite.h"

static double cc_clock()
{
    double clock;
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
    clock = (double)GetTickCount();
#else
    timeval tv;
    gettimeofday(&tv,NULL);
    clock = (double)tv.tv_sec*1000.0 + (double)(tv.tv_usec)/1000.0;
#endif
    return clock;
}

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4
static const GLfloat squareVertices[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    1.0f,  1.0f,
};
static const GLfloat coordVertices[] = {
    0.0f,  1.0f,
    1.0f,  1.0f,
    0.0f,  0.0f,
    1.0f,  0.0f,
};
static const char * _vshader = "\
attribute vec4 position;\n\
attribute vec2 TexCoordIn;\n\
varying vec2 TexCoordOut;\n\
void main(void)\n\
{\n\
    gl_Position = CC_MVPMatrix * position;\n\
    TexCoordOut = TexCoordIn;\n\
}";
static const char * _fshader = "\
varying vec2 TexCoordOut;\n\
uniform sampler2D tex_y;\n\
uniform sampler2D tex_u;\n\
uniform sampler2D tex_v;\n\
uniform float yborder;\n\
uniform float uborder;\n\
uniform float vborder;\n\
void main(void)\n\
{\n\
    vec3 yuv;\n\
    vec3 rgb;\n\
    yuv.x = texture2D(tex_y, vec2(TexCoordOut.x*yborder,TexCoordOut.y)).r;\n\
    yuv.y = texture2D(tex_u, vec2(TexCoordOut.x*uborder,TexCoordOut.y)).r - 0.5;\n\
    yuv.z = texture2D(tex_v, vec2(TexCoordOut.x*vborder,TexCoordOut.y)).r - 0.5;\n\
    rgb = mat3( 1,       1,         1,\n\
               0,       -0.39465,  2.03211,\n\
               1.13983, -0.58060,  0) * yuv;\n\
    gl_FragColor = vec4(rgb, 1);\n\
}";
static const char * _yuv420pShaderName = "yuv420p_shader";

YUVSprite * YUVSprite::createWithYUV420P(int w,int h,uint8_t *yuv[3],int linesize[3])
{
    YUVSprite *sprite = new YUVSprite();
    if(sprite && sprite->initWidthYUV420P(w,h,yuv,linesize)){
        sprite->autorelease();
        return sprite;
    }
    CC_SAFE_DELETE(sprite);
    return NULL;
}

static double t0;
YUVSprite::YUVSprite()
{
    _width = 0;
    _height = 0;
    _prog = NULL;
    textureDone = 0;
    t0 = cc_clock();
}

YUVSprite::~YUVSprite()
{
    if(textureDone){
        glDeleteTextures(1,&id_y);
        glDeleteTextures(1,&id_u);
        glDeleteTextures(1,&id_v);
    //    CCLOG("~YUV %f",cc_clock()-t0);
    }
}

bool YUVSprite::init()
{
    return initWidthYUV420P(0,0,NULL,NULL);
}

bool YUVSprite::initWidthYUV420P(int w,int h,uint8_t *yuv[3],int linesize[3])
{
    /* 初始化shader program */
    //double t0 = cc_clock();
    CCShaderCache * ccsc = CCShaderCache::sharedShaderCache();
    while(1){
        if(ccsc){
            _prog = ccsc->programForKey(_yuv420pShaderName);
            if(!_prog){
                _prog = new CCGLProgram();
            
                if(_prog && _prog->initWithVertexShaderByteArray(_vshader,_fshader) ){
                    //add attribute
                    _prog->addAttribute("position", ATTRIB_VERTEX);
                    _prog->addAttribute("TexCoordIn", ATTRIB_TEXTURE);
                    if(_prog->link()){
                        _prog->updateUniforms();
                        ccsc->addProgram(_prog, _yuv420pShaderName);
                        _prog->release();
                        CHECK_GL_ERROR_DEBUG();
                        break;
                    }else{
                        CCLOG("yuv420p shader program link failed");
                    }
                }else{
                    CCLOG("yuv420p shader program compile failed");
                }
            }else break;
        }
        CCLOG("YUVSprite::initWidthYUV420P yuv420p shader program init failed");
        return false;
    }
    //double t1 = cc_clock();
    /* 加载texture */
    if(CCSprite::init()){
        if(yuv){
            _width = w;
            _height = h;
            
            setContentSize(CCSize(w,h));
            CCRect rect = CCRectZero;
            rect.size = CCSize(w,h);
            setTextureRect(rect,false,rect.size);
            textureUniformY = glGetUniformLocation(_prog->getProgram(), "tex_y");
            textureUniformU = glGetUniformLocation(_prog->getProgram(), "tex_u");
            textureUniformV = glGetUniformLocation(_prog->getProgram(), "tex_v");
            
            borderUniformY = glGetUniformLocation(_prog->getProgram(), "yborder");
            borderUniformU = glGetUniformLocation(_prog->getProgram(), "uborder");
            borderUniformV = glGetUniformLocation(_prog->getProgram(), "vborder");
            
            _border[0] = (float)w/(float)linesize[0];
            _border[1] = (float)w/(2.0f*linesize[1]);
            _border[2] = (float)w/(2.0f*linesize[2]);
           // double t2 = cc_clock();
            glGenTextures(1, &id_y);
            glBindTexture(GL_TEXTURE_2D, id_y);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, linesize[0], h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv[0]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
           // double t3 = cc_clock();
            glGenTextures(1, &id_u);
            glBindTexture(GL_TEXTURE_2D, id_u);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,linesize[1], h/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv[1]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
           // double t4 = cc_clock();
            glGenTextures(1, &id_v);
            glBindTexture(GL_TEXTURE_2D, id_v);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, linesize[2], h/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv[2]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
           // double t5 = cc_clock();
           // CCLOG("prog %f, unif %f , y= %f , u=%f , v=%f",t1-t0,t2-t1,t3-t2,t4-t3,t5-t4);
            CHECK_GL_ERROR_DEBUG();

            textureDone = 1;
        }
        return true;
    }
    return false;
}

void YUVSprite::draw()
{
    CC_PROFILER_START_CATEGORY(kCCProfilerCategorySprite, "YUVSprite - draw");
    
    CCAssert(!m_pobBatchNode, "If YUVSprite is being rendered by CCSpriteBatchNode, YUVSprite#draw SHOULD NOT be called");
   // CC_NODE_DRAW_SETUP();
    ccGLBlendFunc( m_sBlendFunc.src, m_sBlendFunc.dst );

    if(_prog){
        _prog->use();
        _prog->setUniformsForBuiltins();
    }
    if(textureDone){
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, id_y);
        glUniform1i(textureUniformY, 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, id_u);
        glUniform1i(textureUniformU, 1);
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, id_v);
        glUniform1i(textureUniformV, 2);
        
        glUniform1f(borderUniformY,_border[0]);
        glUniform1f(borderUniformU,_border[1]);
        glUniform1f(borderUniformV,_border[2]);
    }
    //ccGLEnableVertexAttribs( kCCVertexAttribFlag_PosColorTex );
    
#define kQuadSize sizeof(m_sQuad.bl)
#ifdef EMSCRIPTEN
    long offset = 0;
    setGLBufferData(&m_sQuad, 4 * kQuadSize, 0);
#else
    long offset = (long)&m_sQuad;
#endif // EMSCRIPTEN
    CHECK_GL_ERROR_DEBUG();
    
    GLfloat square[8];
    CCSize s = this->getTextureRect().size;
    CCPoint offsetPix = this->getOffsetPosition();
    
    square[0] = offsetPix.x;
    square[1] = offsetPix.y;
    
    square[2] = offsetPix.x+s.width;
    square[3] = offsetPix.y;
    
    square[6] = offsetPix.x+s.width;
    square[7] = offsetPix.y+s.height;
    
    square[4] = offsetPix.x;
    square[5] = offsetPix.y+s.height;

    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, square);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, coordVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    /*
    // vertex
    int diff = offsetof( ccV3F_C4B_T2F, vertices);
    glVertexAttribPointer(kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, kQuadSize, (void*) (offset + diff));
    
    // texCoods
    diff = offsetof( ccV3F_C4B_T2F, texCoords);
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, kQuadSize, (void*)(offset + diff));
    
    // color
    diff = offsetof( ccV3F_C4B_T2F, colors);
    glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, (void*)(offset + diff));
    */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    CHECK_GL_ERROR_DEBUG();
#if CC_SPRITE_DEBUG_DRAW == 1
    // draw bounding box
    CCPoint vertices[4]={
        ccp(m_sQuad.tl.vertices.x,m_sQuad.tl.vertices.y),
        ccp(m_sQuad.bl.vertices.x,m_sQuad.bl.vertices.y),
        ccp(m_sQuad.br.vertices.x,m_sQuad.br.vertices.y),
        ccp(m_sQuad.tr.vertices.x,m_sQuad.tr.vertices.y),
    };
    ccDrawPoly(vertices, 4, true);
#elif CC_SPRITE_DEBUG_DRAW == 2
    // draw texture box
    CCSize s = this->getTextureRect().size;
    CCPoint offsetPix = this->getOffsetPosition();
    CCPoint vertices[4] = {
        ccp(offsetPix.x,offsetPix.y), ccp(offsetPix.x+s.width,offsetPix.y),
        ccp(offsetPix.x+s.width,offsetPix.y+s.height), ccp(offsetPix.x,offsetPix.y+s.height)
    };
    ccDrawPoly(vertices, 4, true);
#endif // CC_SPRITE_DEBUG_DRAW
    CC_INCREMENT_GL_DRAWS(1);
    
    CC_PROFILER_STOP_CATEGORY(kCCProfilerCategorySprite, "YUVSprite - draw");
}
