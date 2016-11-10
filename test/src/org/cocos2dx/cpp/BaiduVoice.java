package org.cocos2dx.cpp;

import java.util.ArrayList;

import com.baidu.voicerecognition.android.ui.BaiduASRDigitalDialog;
import com.baidu.voicerecognition.android.ui.DialogRecognitionListener;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.content.Context;

public class BaiduVoice {
	private BaiduASRDigitalDialog mDialog = null;
	private int mCurrentTheme = Config.DIALOG_THEME;
	private DialogRecognitionListener mRecognitionListener;
	private boolean isInit = false;
	
	public interface Result{
		public void onResult( String text );
	}
	public void onBaiduResult( String s ){
	}
	public void show( Context activity){
		if( !isInit ){
			mRecognitionListener = new DialogRecognitionListener() {

	            @Override
	            public void onResults(Bundle results) {
	                ArrayList<String> rs = results != null ? results
	                        .getStringArrayList(RESULTS_RECOGNITION) : null;
	                if (rs != null && rs.size() > 0) {
	                    //mResult.setText(rs.get(0));
	                	onBaiduResult(rs.get(0));
	                }
	            }
	        };
			isInit = true;
		}
//		if (mDialog == null || mCurrentTheme != Config.DIALOG_THEME) {
	        mCurrentTheme = Config.DIALOG_THEME;
	        if (mDialog != null) {
	            mDialog.dismiss();
	        }
	        Bundle params = new Bundle();
	        params.putString(BaiduASRDigitalDialog.PARAM_API_KEY, Constants.API_KEY);
	        params.putString(BaiduASRDigitalDialog.PARAM_SECRET_KEY, Constants.SECRET_KEY);
	        params.putInt(BaiduASRDigitalDialog.PARAM_DIALOG_THEME, Config.DIALOG_THEME);
	        mDialog = new BaiduASRDigitalDialog(activity, params);
	        mDialog.setDialogRecognitionListener(mRecognitionListener);
//	    }
	    mDialog.getParams().putInt(BaiduASRDigitalDialog.PARAM_PROP, Config.CURRENT_PROP);
	    mDialog.getParams().putString(BaiduASRDigitalDialog.PARAM_LANGUAGE,
	            Config.getCurrentLanguage());
	    Log.e("DEBUG", "Config.PLAY_START_SOUND = "+Config.PLAY_START_SOUND);
	    mDialog.getParams().putBoolean(BaiduASRDigitalDialog.PARAM_START_TONE_ENABLE, Config.PLAY_START_SOUND);
	    mDialog.getParams().putBoolean(BaiduASRDigitalDialog.PARAM_END_TONE_ENABLE, Config.PLAY_END_SOUND);
	    mDialog.getParams().putBoolean(BaiduASRDigitalDialog.PARAM_TIPS_TONE_ENABLE, Config.DIALOG_TIPS_SOUND);
	    mDialog.show();
	}
	public void showConfig(Context activity){
        Intent setting = new Intent(activity, BaiduVoiceSettingActivity.class);
        activity.startActivity(setting);
	}
	public void destory(){
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
	}
}
