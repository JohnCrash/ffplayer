/*
 * 使用opengl shader直接绘制yuv420p图像
 */

#ifndef YUV_SPRITE_h
#define YUV_SPRITE_h

using namespace cocos2d;

class YUVSprite : public CCSprite
{
public:
    static YUVSprite * createWithYUV420P(int w,int h,uint8_t *yuv[3],int linesize[3]);
    
    YUVSprite();
    
    virtual ~YUVSprite();
    
    virtual bool init();
    
    virtual bool initWidthYUV420P(int w,int h,uint8_t *yuv[3],int linesize[3]);
    
    virtual void draw();
private:
    int _width,_height;
    float _border[3];
    CCGLProgram * _prog;
    GLuint id_y;
    GLuint id_u;
    GLuint id_v;
    GLuint textureUniformY;
    GLuint textureUniformU;
    GLuint textureUniformV;
    GLuint borderUniformY;
    GLuint borderUniformU;
    GLuint borderUniformV;
    int textureDone;
};

#endif /* Header_h */
