package org.cocos2dx.cpp;

import com.baidu.voicerecognition.android.ui.BaiduASRDigitalDialog;

import android.R;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.Spinner;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.CompoundButton.OnCheckedChangeListener;

public class BaiduVoiceSettingActivity extends Activity implements OnCheckedChangeListener {

	    private Spinner propTypeSpinner;

	    private Spinner dialogThemeSpinner;

	    private Spinner languageSpinner;

	    private CheckBox startSoundCheckBox;

	    private CheckBox endSoundCheckBox;
	    
	    private CheckBox dialogTipsCheckBox;

	    private CheckBox showVolCheckBox;

	    @Override
	    protected void onCreate(Bundle savedInstanceState) {
	        super.onCreate(savedInstanceState);
	    }

	    @Override
	    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
	        if (buttonView == showVolCheckBox) {
	            Config.SHOW_VOL = isChecked;
	        }
	        if (buttonView == startSoundCheckBox) {
	            Config.PLAY_START_SOUND = isChecked;
	        }
	        if (buttonView == endSoundCheckBox) {
	            Config.PLAY_END_SOUND = isChecked;
	        }
	        if (buttonView == dialogTipsCheckBox) {
	            Config.DIALOG_TIPS_SOUND = isChecked;
	        }
	    }
	}
