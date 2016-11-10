/****************************************************************************
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
Copyright (c) 2013-2014 Chukong Technologies Inc.
 
http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
package org.cocos2dx.cpp;

import java.security.Provider;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.provider.MediaStore;
import android.view.KeyEvent;

import java.io.File;
import java.lang.System;

import android.net.Uri;
import android.app.Activity;
import android.app.AlertDialog;

import org.acra.ACRA;
import org.cocos2dx.lib.Cocos2dxActivity;
import org.cocos2dx.lib.Cocos2dxGLSurfaceView;
import org.cocos2dx.lib.Cocos2dxHelper;
import org.cocos2dx.lib.Cocos2dxCallback;

import android.graphics.Bitmap;
import android.os.Build;
import android.database.Cursor;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.content.BroadcastReceiver;
import android.os.Vibrator;

import android.app.AlertDialog;
import android.content.DialogInterface;

import org.acra.*;
import org.acra.annotation.*;
import org.ffmpeg.device.AndroidDemuxer;

//import org.cocos2dx.cpp.CrashHandler;
@ReportsCrashes()
public class AppActivity extends Cocos2dxActivity  implements Cocos2dxCallback{
	//======================
	// JNI
	//======================
	private static native void networkStateChangeEvent(int state);
	private static native void launchParam(final String launch,final String cookie,final String uid,final String orientation);
	private static native void setExternalStorageDirectory(final String sd);
	private static native void sendTakeResourceResult(int resultCode,int typeCode,final String res); 
	private static native void sendVoiceRecordData(final int nType,final int nID,final int nParam1,final int nParam2,final int len,final byte[] pBytes);
	private static native void cocos2dChangeOrientation( final int state,final int w,final int h);
	private static native void setBaiduResult( final String text );
	public static native void writeACRLog( final String text );
	//======================
	// 拍照和取图库
	//======================
	private static final int RETURN_TYPE_RECORDDATA = 10;
	private static final int TAKE_PICTURE = 1;
	private static final int PICK_PICTURE = 2;
	private static AppActivity myActivity;
	private static String _resourceName;
	public static void takeResource(int from)
	{
		if( from == 1 )
		{
			String storageState = Environment.getExternalStorageState();
			if(storageState.equals(Environment.MEDIA_MOUNTED) )
			{
				Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
				long ct = System.currentTimeMillis();
				_resourceName = Environment.getExternalStorageDirectory().getPath() + File.separatorChar + "ljdata/EDEngine/tmp" + ct + ".jpg";
				File file = new File(_resourceName);
				Uri fileUri = Uri.fromFile(file);
				intent.putExtra( MediaStore.EXTRA_OUTPUT, fileUri);
				intent.putExtra("outputFormat", Bitmap.CompressFormat.JPEG.toString());
				myActivity.startActivityForResult(intent,TAKE_PICTURE);
			}
			else
			{
				new AlertDialog.Builder(myActivity)
	            .setMessage("External Storeage (SD Card) is required.\n\nCurrent state: " + storageState)
	            .setCancelable(true).create().show();			
			}
		}else if(from==2)
		{
			Intent intent=new Intent();
			if (Build.VERSION.SDK_INT<19)
			{
				intent.setAction(Intent.ACTION_GET_CONTENT);
				intent.setType("image/*");
			}
			else
			{
				intent.setAction(Intent.ACTION_PICK);
				intent.setData(android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
			}
			myActivity.startActivityForResult(intent,PICK_PICTURE);			
		}else if(from==3)
		{
			//record audio
		}
	}
@Override
	public void onActivityResult(int requestCode,int resultCode,Intent data)
	{
		if(requestCode == TAKE_PICTURE || requestCode == PICK_PICTURE )
		{
			if( requestCode==PICK_PICTURE )
			{
				try
				{
					Uri uri = data.getData();
					Cursor cursor = getContentResolver().query(uri, new String[]{"_data"}, null, null, null);
					String strPathName;
					strPathName=uri.getPath();
					if (cursor==null)
					{
						resultCode = 0; //cancel
					}
					else
					{
						cursor.moveToFirst();
						strPathName=cursor.getString(0);
					}
					sendTakeResourceResult(requestCode,resultCode,strPathName);
					return;
				}
				catch(Exception e)
				{
				}
			}
			sendTakeResourceResult( requestCode,resultCode,_resourceName);
			return;
		}
		super.onActivityResult(requestCode, resultCode, data);
	}
	//======================
	//录音
	//======================
	private static Thread s_thread;
	private static boolean s_bRecording=false;
	private static int s_nRecordState=0;
	
	private static int s_cnChannel;
	private static int s_nRate;
	private static int s_cnBitPerSample;
	
	private static int[] s_rateList={8000,11025,16000,22050,44100};
	private static int s_validRate=0;
	
	public static void RecordThreadFunc()
	{
		//转换为android识别值
		int nChannel=s_cnChannel==1 ? AudioFormat.CHANNEL_CONFIGURATION_MONO : AudioFormat.CHANNEL_IN_STEREO;
		int audioEncoding=s_cnBitPerSample==8 ? AudioFormat.ENCODING_PCM_8BIT : AudioFormat.ENCODING_PCM_16BIT;
	
	    if (s_validRate==0)
	    {
	    	for (int i=0;i<5;i++)
	    	{
	    		int nRateThis=s_rateList[i];
	    		int minSize=AudioRecord.getMinBufferSize(nRateThis,nChannel,audioEncoding)*5;
	    		if (minSize<0) continue;
	    		
	    		AudioRecord audioRecord=new AudioRecord(MediaRecorder.AudioSource.MIC,nRateThis,nChannel,audioEncoding,minSize);
	    		if (audioRecord.getState() == AudioRecord.STATE_INITIALIZED)
	    		{
	    			Log.w("AudioRecord","valid rate test successed");
	    			audioRecord.stop();
	    			audioRecord.release();
	    			s_validRate=nRateThis;
	    			break;
	    		}
				audioRecord.stop();
				audioRecord.release();
	    	}
	        if (s_validRate==0)
	    	{
				//错误结束
				s_nRecordState=-1;
				s_bRecording=false;
				return;
	    	}
	        s_nRate=s_validRate;
	    }
	    int bufferSize=AudioRecord.getMinBufferSize(s_nRate,nChannel,audioEncoding)*2;
	    AudioRecord audioRecord=new AudioRecord(MediaRecorder.AudioSource.MIC,s_nRate,nChannel,audioEncoding,bufferSize);
	    if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED)
	    {
			Log.w("AudioRecord","init fail");
			//错误结束
			audioRecord.stop();
			audioRecord.release();
			s_nRecordState=-1;
			s_bRecording=false;
			return;
	    }
	      
	    final byte[] buffer=new byte[bufferSize];
	    //开始录音
	    audioRecord.startRecording();
	
		while (s_bRecording)
		{
			//这个函数会阻塞吗？否则如果没有数据会快速循环。这里可能会出现奇数值啊？比如单声道加8bit
			int bufferReadResult=audioRecord.read(buffer,0,bufferSize);
	
			if (bufferReadResult<0)
			{
				//Log.w("stop","RecordThreadFunc stop");
				//停止录音
				audioRecord.stop();
				audioRecord.release();
				s_nRecordState=-2;
				s_bRecording=false;
				return;
			}
			myActivity.sendVoiceRecordData(RETURN_TYPE_RECORDDATA,0,s_nRate,0,bufferReadResult,buffer);
			//Cocos2dxHelper.SendJavaReturnBufDirectly(RETURN_TYPE_RECORDDATA,0,s_nRate,0,bufferReadResult,buffer);
		}
		//停止录音
		audioRecord.stop();
		audioRecord.release();
	
		//已经不录音了，正常结束
		s_nRecordState=1;
		s_bRecording=false;
		return;
	}
	/*
	public static int CheckRecordPrivilege()
	{
		StringBuffer appNameAndPermissions=new StringBuffer();
		PackageManager pm=getContext().getPackageManager();
		List<ApplicationInfo> packages=pm.getInstalledApplications(PackageManager.GET_META_DATA);
	
		for (ApplicationInfo applicationInfo : packages)
		{
			try
			{
				PackageInfo packageInfo = pm.getPackageInfo("com.lj.ljshell", PackageManager.GET_PERMISSIONS);
				appNameAndPermissions.append(packageInfo.packageName+"*:\n");
	
				//Get Permissions
				String[] requestedPermissions = packageInfo.requestedPermissions;
				if (requestedPermissions != null)
				{
					for (int i = 0; i < requestedPermissions.length; i++)
					{
						Log.d("test", requestedPermissions[i]);
						appNameAndPermissions.append(requestedPermissions[i]+"\n");
					}
					appNameAndPermissions.append("\n");
				}
			}
			catch (NameNotFoundException e)
			{
				e.printStackTrace();
			}
		}
		return 1;
	}
	*/	
	public static int VoiceStartRecord(int cnChannel,int nRate,int cnBitPerSample)
	{
		//if (CheckRecordPrivilege()==0) return 0;
		//Log.d("test"," j_VoiceStartRecord ");
		if (s_bRecording)
		{
			//如果正在录音，先要求停止
			if (VoiceStopRecord()==0) return 0;
		}
	
		//1=单声道，2=立体声
		s_cnChannel=cnChannel;
		
		//采样频率，8000...
		s_nRate=nRate;
		
		//每次采样的数据位数，8或16
		s_cnBitPerSample=cnBitPerSample;
	
		//Log.d("test"," j_VoiceStartRecord new thread");
		//由独立线程录制
		s_thread=new Thread(new Runnable()
		{
	        public void run()
	        {
	        	//实际的线程函数
	        	RecordThreadFunc();
	        }    
		});
		//设置状态
		s_nRecordState=0;
		s_bRecording=true;
		s_thread.start();
		try
		{
			//等待50毫秒
			Thread.sleep(50);
		}
		catch (Exception e)
		{
		}
		if (s_nRecordState<0) return 0;
		return 1;
	}
	
	public static int VoiceStopRecord()
	{
		//停止录音标志，线程循环时看到会停止
		s_bRecording=false;
		//Log.w("stop","VoiceStopRecord");
		//最多等待100毫秒，让线程自己终止
		for (int i=0;i<500;i++)
		{
			//线程已经不在录音了
			if (s_nRecordState!=0) break;
			
			try
			{
				//等待10毫秒
				Thread.sleep(10);
			}
			catch (Exception e)
			{
				//错误
				s_nRecordState=-1;
				break;
			}
		}
		//Log.w("stop","VoiceStopRecord 2");
		//有错误，也有可能是线程设置的
		if (s_nRecordState<=0)
		{
			Log.w("record","can not stop the thread");
			return 0;
		}
	
		//Log.w("stop","VoiceStopRecord 3");
		return 1;
	}
	//========================
	// VoiceStartPlay
	//========================
	private static String s_strPathNamePlaying;
	private static MediaPlayer s_mediaPlayer;
	private static boolean s_bPlaying=false;

	public static int VoiceStartPlay(String strPathName)
	{
		if (s_bPlaying)
		{
			s_mediaPlayer.stop();
			s_mediaPlayer.release();
			s_bPlaying=false;
		}
		s_strPathNamePlaying=strPathName;
		s_mediaPlayer=new MediaPlayer();
		
		try
		{
			s_mediaPlayer.setDataSource(strPathName);
			s_mediaPlayer.prepare();
			s_mediaPlayer.start();
		}
		catch(Exception e)
		{
			s_mediaPlayer.stop();
			s_mediaPlayer.release();

			return 0;
		}
		s_bPlaying=true;

		s_mediaPlayer.setOnCompletionListener(new OnCompletionListener()
		{  
            public void onCompletion(MediaPlayer mp)
            {
            	mp.stop();
                mp.release();  
                s_bPlaying=false;
            }  
        });
		
		return 1;
	}

	public static int VoiceIsPlaying(String strPathName)
	{
		if (!s_bPlaying) return 0;
		
		if (strPathName.length()==0) return 1;
		if (strPathName.equals(s_strPathNamePlaying)) return 1;
		
		return 0;
	}
	
	public static void Buy(String itemName)
	{
		
	}
	
	public static int VoiceStopPlay()
	{
		if (s_bPlaying)
		{
			s_mediaPlayer.stop();
			s_mediaPlayer.release();
			s_bPlaying=false;
		}
		return 1;
	}
	/*
	 * 震动
	 */
	private static Vibrator vibrator;
	private static long [] s_pattern = null;
	private static int s_patidx = 0;
	//使用两个函数绕过传递数组
	public static void InitShockPattern( long p )
	{
		if( s_pattern == null )
			s_pattern = new long[64];
		s_patidx = 1;
		s_pattern[0] = p;
	}
	public static void addShockPattern( long p )
	{
		if( s_patidx < 64 )
		{
			s_pattern[s_patidx] = p;
			s_patidx++;
		}
	}
	public static void ShockPhone()
	{
		if( vibrator == null)
			vibrator = (Vibrator)myActivity.getSystemService(Context.VIBRATOR_SERVICE);
		vibrator.vibrate(s_pattern, s_patidx);
	}
	public static void ShockPhoneDelay( long time )
	{
		if( vibrator == null)
			vibrator = (Vibrator)myActivity.getSystemService(Context.VIBRATOR_SERVICE);
		vibrator.vibrate(time);		
	}
	public static void stopShockPhone()
	{
		if( vibrator != null)
			vibrator.cancel();
	}
	/*
	 *  取网络状态和监控网络状态
	 *  0没有网络,1 wifi,2 mobile
	 */
	public static int getNetworkState()
	{
		ConnectivityManager cmgr = (ConnectivityManager)myActivity.getSystemService(CONNECTIVITY_SERVICE);
		if( cmgr == null )
			return 0;
		NetworkInfo networkInfo = cmgr.getActiveNetworkInfo();
		if( networkInfo==null )
			return 0; //没有网络
		NetworkInfo wifiInfo = cmgr.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
		if( wifiInfo != null )
		{
			if(  wifiInfo.isAvailable() )
				return 1;
		}
		
		NetworkInfo mobiInfo = cmgr.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
		if( mobiInfo != null )
		{
			if( mobiInfo.isConnected() )
				return 2;
		}
		
		return 0;
	}
	static BroadcastReceiver connectionReceiver;
	public static void registerNetworkStateListener()
	{
		if( connectionReceiver == null )
		{
			connectionReceiver =  new BroadcastReceiver() { 
				@Override 
				public void onReceive(android.content.Context context, Intent intent) {
					int state = getNetworkState();
					networkStateChangeEvent( state );
				}
			};
		}
		if( connectionReceiver != null )
		{
			IntentFilter intf = new IntentFilter();
			intf.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
			myActivity.registerReceiver(connectionReceiver,intf);
		}
		else
		{
			Log.w("registerNetworkStateListener","connectionReceiver==null");
		}
	}
	public static void unregisterNetworkStateListener()
	{
		if( connectionReceiver != null )
			myActivity.unregisterReceiver(connectionReceiver);
	}
	/*
	 *  openURL
	 */
	public static void androidOpenURL( String url )
	{
		Log.d("androidOpenURL",url);
		Intent intent = new Intent(Intent.ACTION_VIEW);
		intent.setData(Uri.parse(url));
		if (intent.resolveActivity(myActivity.getPackageManager()) != null) {
			myActivity.startActivity(intent);
		}
	}
	/*
	 * 切换方向
	 */
	public int _isSetUIOrientation = -1;

	public static void setUIOrientation( int m )
	{
		int orientation = myActivity.getRequestedOrientation();
		if( m == 1 )
		{ //横屏
			 if(orientation !=ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE){
				 myActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
			}
			else
			 {
				cocos2dChangeOrientation(0,0,0);
			 }
		}else if( m == 2 )
		{ //竖屏
			 if(orientation !=ActivityInfo.SCREEN_ORIENTATION_PORTRAIT){
				 myActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
			}
			else
			 {
				cocos2dChangeOrientation(0,0,0);
			 }
		}else
		{
			cocos2dChangeOrientation(0,0,0);
		}
		myActivity._isSetUIOrientation = 1;
	}
	//@Override
	public void onSizeChanged( int width,int height )
	{
	//	String msg;
		
	//	msg = String.format("onSizeChanged width = %d,height = %d",width,height);
	//	Log.w("onSizeChanged",msg);
	//	try{
	//		Thread.currentThread().sleep(200);
	//	}catch(InterruptedException e){
	//		
	//	}
	//	if(_isSetUIOrientation == 1)
		{
			//发生屏幕旋转
			Log.w("onSizeChanged",String.format("(%d , %d)",width,height));
			cocos2dChangeOrientation( 1,width,height );
			Log.w("onSizeChanged after",String.format("(%d , %d)",width,height));
		}
		_isSetUIOrientation = -1;
	}
	
	public void onDrawFrame()
	{
		AndroidDemuxer.requestGLThreadProcess();
	}
	
	public static int getUIOrientation()
	{
		int orientation = myActivity.getRequestedOrientation();
		int m = 1;
		if(orientation !=ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)
			m = 1;
		else if(orientation !=ActivityInfo.SCREEN_ORIENTATION_PORTRAIT)
			m = 2;
		return m;
	}
	public void onConfigurationChanged( Configuration  newConfig)
	{
		super.onConfigurationChanged(newConfig);
	    if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
	    	//do..
	    	Log.w("onConfigurationChanged","SCREEN_ORIENTATION_LANDSCAPE");
	    }
	    else {
	    	//do..
	    	Log.w("onConfigurationChanged","SCREEN_ORIENTATION_PORTRAIT");
	    }
	}
	private static String _launch;
	private static String _cookie;
	private static String _orientation;
	private static String _uid;
	//========================
	// 传递参数
	//========================
	public void getParameterByIntent() {
		Intent mIntent = this.getIntent();  
		_launch = mIntent.getStringExtra("launch");
		_cookie = mIntent.getStringExtra("cookie");
		_orientation = mIntent.getStringExtra("orientation");
		int userid =  mIntent.getIntExtra("userid",0);
		_uid = Integer.toString(userid);
		String path = Environment.getExternalStorageDirectory().getPath();
		if( path.length()>0 && path.charAt(path.length()-1)!='/')
			path += '/';
		setExternalStorageDirectory( path );
		launchParam(_launch,_cookie,_uid,_orientation);
		/*
		 * 根据启动参数来确定横屏还是竖屏，如果是orientation==landscape将设置横屏
		 * lua代码同样需要取得方向参数来确定屏幕方向
		 */
		Log.w("orientation","=====================================\n");
		if( _orientation!=null )
			Log.w("orientation","orientation:"+_orientation.toString());
		else
			Log.w("orientation","orientation:null");
		Log.w("orientation","=====================================\n");
		if(_orientation!=null && _orientation.equalsIgnoreCase("portrait")){
			Log.w("orientation","SCREEN_ORIENTATION_PORTRAIT");
			myActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		}
		else{
			Log.w("orientation","SCREEN_ORIENTATION_LANDSCAPE");
			myActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		}
		//launchParam("errortitile","sc1=D3F1DC81D98457FE8E1085CB4262CAAD5C443773akl%2bNQbvBYOcjHsDK0Fu4kV%2fbgv3ZBi7sFKU19KP5ks0GkvPwGpmMWe%2b8Q6O%2fkT7EuHjkQ%3d%3d");
		}
	/*
	public void setParameterByIntent(String pkg,String cls,String param1,String param2) 
	{
		ComponentName componentName = new ComponentName(pkg,cls); 
		Intent intent = new Intent();  
		Bundle bundle = new Bundle();  
		bundle.putString("launch", param1);
		bundle.putString("cookie", param2); 
		intent.putExtras(bundle);  
		intent.setComponent(componentName);  
		startActivity(intent);
	}*/
    private void errorBox(String title,String info){
		AlertDialog.Builder builder = new AlertDialog.Builder(AppActivity.this);
		builder.setTitle(title);
		builder.setMessage(info);
		builder.setNegativeButton("确定", new DialogInterface.OnClickListener(){
			@Override
			public void onClick(DialogInterface dialog, int which){
				//settingListActivity.this.finish();
				System.exit(0);
			}
		});
		AlertDialog alertDialog = builder.create();
		alertDialog.setCancelable(false);
		/*
		 * 防止对话框被cancel  
		 */
		alertDialog.setOnKeyListener(new DialogInterface.OnKeyListener() {
			   @Override
			   public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event)
			   {
			   if (keyCode == KeyEvent.KEYCODE_SEARCH)
			    {
			     return true;
			    }
			    else
			    {
			     return false; //默认返回 false
			    }
			   }
		});		
		alertDialog.show(); 
    }
    
    //copy from ljshell
	public static int StartApp(String strPackage,String strPath,String strParam)
    {
		int nRet=1;
		boolean bSingleTop=true;
		try
		{
			String strClassName="";
			
			Bundle bundle = new Bundle();  
			if (strPackage.equalsIgnoreCase("com.edengine.luacore"))
			{
				//for lua
				strClassName="org.cocos2dx.cpp.AppActivity";
				
				bundle.putString("launch",strPath);
				String str = "";
				/*
				String str=FindParam(strParam, "userid");
				if (!str.isEmpty())
				{
					bundle.putInt("userid",Integer.parseInt(str));
				}
				
				str=FindParam(strParam, "token");
				if (!str.isEmpty())
				{
					str="sc1="+str;
					bundle.putString("cookie",str);
				}

				str=FindParam(strParam, "devicetype");
				if (!str.isEmpty())
				{
					bundle.putInt("devicetype",Integer.parseInt(str));
				}

				str=FindParam(strParam, "deviceid");
				if (!str.isEmpty())
				{
					bundle.putString("deviceid",str);
				}
				*/
				int n=0;
				while (n<strParam.length())
				{
					int m=strParam.indexOf('=', n);
					if (m<0) break;
					
					String strParamName=strParam.substring(n,m);
					m++;
					n=strParam.indexOf('&',m);
					String strParamValue;
					if (n<0) strParamValue=strParam.substring(m);
					else strParamValue=strParam.substring(m,n);

					if (strParamName.equals("userid"))
					{
						bundle.putInt(strParamName,Integer.parseInt(strParamValue));
					}
					else if (strParamName.equals("token"))
					{
						str = "sc1="+strParamValue;
						bundle.putString("cookie",str);
					}
					else if (strParamName.equals("devicetype"))
					{
						bundle.putInt("devicetype",Integer.parseInt(strParamValue));
					}
					else bundle.putString(strParamName,strParamValue);
					if (n<0) break;
					n++;
				}
			}
			else
			{
				bSingleTop=false;
				int n=0;
				while (n<strParam.length())
				{
					int m=strParam.indexOf('=', n);
					if (m<0) break;
					
					String strParamName=strParam.substring(n,m);
					m++;
					n=strParam.indexOf('&',m);
					String strParamValue;
					if (n<0) strParamValue=strParam.substring(m);
					else strParamValue=strParam.substring(m,n);

					if (strParamName.equals("class")) strClassName=strParamValue;
					else if (strParamName.equals("singletop")) bSingleTop=Integer.parseInt(strParamValue)==1;
					else bundle.putString(strParamName,strParamValue);

					if (n<0) break;
					n++;
				}
			}

			Intent intent;
			if (strClassName.isEmpty())
			{
				intent = getContext().getPackageManager().getLaunchIntentForPackage(strPackage);
			}
			else
			{
				intent=new Intent();  
				ComponentName componentName = new ComponentName(strPackage,strClassName); 
				intent.setComponent(componentName);
			}
			intent.putExtras(bundle);  
			
			if (bSingleTop) intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
			getContext().startActivity(intent);
		}
		catch (Exception e)
		{
			Log.w("StartApp",strPackage);
			nRet=0;
		}
		return nRet;
	}
	
    public Cocos2dxGLSurfaceView onCreateView() {
		//CrashHandler crashHandler = CrashHandler.getInstance();  
        //crashHandler.init(getApplicationContext());
    	ACRAConfiguration config = ACRA.getConfig();
    	ACRA.init(this.getApplication(),config);
    	ACRASender mySender = new ACRASender(this);
    	ACRA.getErrorReporter().setReportSender(mySender);
    	myActivity = this;
    	getParameterByIntent(); //取启动参数
    	/*
    	if(_launch==null || _cookie==null){
    		Intent homeIntent = new Intent(Intent.ACTION_MAIN);
    		if(StartApp("com.lj.ljshell","","")==0)
    			errorBox("提示","亲爱的用户，想要使用“乐学园地”，请先在步步高应用商店中，搜索并安装《乐教乐学》.在其他应用商店安装也可以哟！");
    		else
    			System.exit(0);
    	}
    	*/
        Cocos2dxGLSurfaceView glSurfaceView = new Cocos2dxGLSurfaceView(this);
        glSurfaceView.setActivityCallback( this );
        // TestCpp should create stencil buffer
        glSurfaceView.setEGLConfigChooser(5, 6, 5, 0, 16, 8);
        //glSurfaceView.setEGLConfigChooser(8, 8, 8, 8, 16, 8);
//Android SDK Document        
//        setEGLConfigChooser(int redSize, int greenSize, int blueSize, int alphaSize, int depthSize, int stencilSize)
//        Install a config chooser which will choose a config with at least the specified depthSize and stencilSize, and exactly the specified redSize, greenSize, blueSize and alphaSize.
        return glSurfaceView;
    }
    /*
     * 加入百度语音支持
     */
    private BaiduVoice _BaiduVoice  = new BaiduVoice(){
		@Override
		public void onBaiduResult( String t ){
			setBaiduResult( t );
		}
    };
    private static int SHOW_BAIDU_VOICE = 1;
    private static int SHOW_BAIDU_VOICE_CONFIGURE = 2;
    private static int SHOW_BAIDU_VOICE_CLOSE = 3;
    private Handler _handler = new Handler(){
    	@Override
    	public void handleMessage(final Message msg){
    		if( msg.what == SHOW_BAIDU_VOICE ){
    	    	myActivity._BaiduVoice.show( myActivity );
    		}else if( msg.what == SHOW_BAIDU_VOICE_CONFIGURE){
    	    	myActivity._BaiduVoice.showConfig(myActivity);      			
    		}else if( msg.what == SHOW_BAIDU_VOICE_CLOSE  ){
    	    	myActivity._BaiduVoice.destory();
    		}
    	}
    };    
    public static void showBaiduVoice(){
		Message msg = new Message();
		msg.what = SHOW_BAIDU_VOICE;
		myActivity._handler.sendMessage(msg);
    }
    public static void closeBaiduVoice(){
		Message msg = new Message();
		msg.what = SHOW_BAIDU_VOICE_CLOSE;
		myActivity._handler.sendMessage(msg); 	
    }
    public static void showBaiduVoiceConfigure(){
		Message msg = new Message();
		msg.what = SHOW_BAIDU_VOICE_CONFIGURE;
		myActivity._handler.sendMessage(msg); 	
    }
    @Override
    protected void onDestroy(){
    	if( _BaiduVoice != null )
    		_BaiduVoice.destory();
    	super.onDestroy();
    }
    // Audio
    protected static AudioTrack mAudioTrack;
    
    // Audio
    public static int audioInit(int sampleRate, boolean is16Bit, boolean isStereo, int desiredFrames) {
        int channelConfig = isStereo ? AudioFormat.CHANNEL_CONFIGURATION_STEREO : AudioFormat.CHANNEL_CONFIGURATION_MONO;
        int audioFormat = is16Bit ? AudioFormat.ENCODING_PCM_16BIT : AudioFormat.ENCODING_PCM_8BIT;
        int frameSize = (isStereo ? 2 : 1) * (is16Bit ? 2 : 1);
        
        Log.v("SDL", "SDL audio: wanted " + (isStereo ? "stereo" : "mono") + " " + (is16Bit ? "16-bit" : "8-bit") + " " + (sampleRate / 1000f) + "kHz, " + desiredFrames + " frames buffer");
        
        // Let the user pick a larger buffer if they really want -- but ye
        // gods they probably shouldn't, the minimums are horrifyingly high
        // latency already
        desiredFrames = Math.max(desiredFrames, (AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat) + frameSize - 1) / frameSize);
        
        if (mAudioTrack == null) {
            mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate,
                    channelConfig, audioFormat, desiredFrames * frameSize, AudioTrack.MODE_STREAM);
            
            // Instantiating AudioTrack can "succeed" without an exception and the track may still be invalid
            // Ref: https://android.googlesource.com/platform/frameworks/base/+/refs/heads/master/media/java/android/media/AudioTrack.java
            // Ref: http://developer.android.com/reference/android/media/AudioTrack.html#getState()
            
            if (mAudioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
                Log.e("SDL", "Failed during initialization of Audio Track");
                mAudioTrack = null;
                return -1;
            }
            
            mAudioTrack.play();
        }
       
        Log.v("SDL", "SDL audio: got " + ((mAudioTrack.getChannelCount() >= 2) ? "stereo" : "mono") + " " + ((mAudioTrack.getAudioFormat() == AudioFormat.ENCODING_PCM_16BIT) ? "16-bit" : "8-bit") + " " + (mAudioTrack.getSampleRate() / 1000f) + "kHz, " + desiredFrames + " frames buffer");
        
        return 0;
    }
    
    public static void audioWriteShortBuffer(short[] buffer)
    {
        for (int i = 0; i < buffer.length; )
        {
            int result = mAudioTrack.write(buffer, i, buffer.length - i);
            if (result > 0)
            {
                i += result;
            }
            else if (result == 0)
            {
                try
                {
                    Thread.sleep(1);
                }
                catch(InterruptedException e)
                {
                    // Nom nom
                }
            }
            else
            {
                Log.w("SDL", "SDL audio: error return from write(short)");
                return;
            }
        }
    }
    
    public static void audioWriteByteBuffer(byte[] buffer)
    {
        for (int i = 0; i < buffer.length; )
        {
            int result = mAudioTrack.write(buffer, i, buffer.length - i);
            if (result > 0)
            {
                i += result;
            }
            else if (result == 0)
            {
                try
                {
                    Thread.sleep(1);
                }
                catch(InterruptedException e)
                {
                    // Nom nom
                }
            } else {
                Log.w("SDL", "SDL audio: error return from write(byte)");
                return;
            }
        }
    }

    public static void audioQuit()
    {
        if (mAudioTrack != null)
        {
            mAudioTrack.stop();
            mAudioTrack = null;
        }
    }	   
}