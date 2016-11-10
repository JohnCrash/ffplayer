package org.cocos2dx.cpp;

import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import org.acra.collector.CrashReportData;
import org.acra.sender.ReportSender;
import org.acra.sender.ReportSenderException;
import org.acra.ACRA;
import org.acra.ACRAConstants;
import org.acra.ReportField;

import android.content.Context;
import android.os.Environment;

import android.util.Log;
public class ACRASender implements ReportSender {
	private final Context mContext;
	public ACRASender(Context ctx){
		mContext = ctx;
	}
	@Override
	public void send(Context arg0, CrashReportData arg1)
			throws ReportSenderException {
		/*
		 * write to dump file
		 */
		String dumpString = buildBody(arg1);
		try{
			FileOutputStream stream = mContext.openFileOutput("crash.dump",Context.MODE_PRIVATE);
			stream.write(dumpString.getBytes());
			stream.flush();
			stream.close();
			/*
			 * write lua log
			 */
			String logfile = mContext.getFilesDir().getPath();
			AppActivity.writeACRLog(logfile+"/crash.log");
		}catch(FileNotFoundException e){
		}catch(IOException e){
		}
	}
    private String buildBody(CrashReportData errorContent) {
        ReportField[] fields = ACRA.getConfig().customReportContent();
        if(fields.length == 0) {
            fields = ACRAConstants.DEFAULT_MAIL_REPORT_FIELDS;
        }

        final StringBuilder builder = new StringBuilder();
        for (ReportField field : fields) {
            builder.append(field.toString()).append("=");
            builder.append(errorContent.get(field));
            builder.append('\n');
        }
        return builder.toString();
    }
}
