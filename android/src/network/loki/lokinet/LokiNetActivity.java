package network.loki.lokinet;


import java.io.File;
import java.io.InputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.URL;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.app.Activity;

import android.content.Context;

import android.content.ComponentName;
import android.content.ServiceConnection;

import android.os.AsyncTask;
import android.content.Intent;
import android.os.Bundle;

import android.os.IBinder;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.TextView;


public class LokiNetActivity extends Activity {
	private static final String TAG = "lokinet-activity";
	private TextView textView;
	private static final String DefaultBootstrapURL = "https://i2p.rocks/bootstrap.signed";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// copy assets
		//String conf = copyFileAsset("daemon.ini");
		textView = new TextView(this);
		setContentView(textView);

		Lokinet_JNI.loadLibraries();
	}


	private static void writeFile(File out, InputStream instream) throws IOException {
		OutputStream outstream = new FileOutputStream(out);
		byte[] buffer = new byte[512];
		int len;
		try {
			do {
				len = instream.read(buffer);
				if (len > 0) {
					outstream.write(buffer, 0, len);
				}
			}
			while (len != -1);
		} finally {
			outstream.close();
		}
	}

	public void startLokinet() {

	}

	public void runLokinetService()
	{
		bindService(new Intent(LokiNetActivity.this,
				ForegroundService.class), mConnection, Context.BIND_AUTO_CREATE);
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		textView = null;
	}

	private class AsyncBootstrap extends AsyncTask<String, String, String>
	{
		public String doInBackground(String ... urls) {
			try
			{
				File bootstrapFile = new File(getCacheDir(), "bootstrap.signed");
				URL bootstrapURL = new URL(urls[0]);
				InputStream instream = bootstrapURL.openStream();
				writeFile(bootstrapFile, instream);
				instream.close();
				return "downloaded";
			}
			catch(Exception thrown)
			{
				return thrown.getLocalizedMessage();
			}
		}
		public void onPostExecute(String val) {
			final File configFile = new File(getCacheDir(), "daemon.ini");
			runLokinetService();
		}
	}

	private CharSequence throwableToString(Throwable tr) {
		StringWriter sw = new StringWriter(8192);
		PrintWriter pw = new PrintWriter(sw);
		tr.printStackTrace(pw);
		pw.close();
		return sw.toString();
	}

	private ForegroundService boundService;
	// private LocalService mBoundService;
	
	private ServiceConnection mConnection = new ServiceConnection() {
		public void onServiceConnected(ComponentName className, IBinder service) {
			// This is called when the connection with the service has been
			// established, giving us the service object we can use to
			// interact with the service.  Because we have bound to a explicit
			// service that we know is running in our own process, we can
			// cast its IBinder to a concrete class and directly access it.
			boundService = ((ForegroundService.LocalBinder)service).getService();
			textView.setText(R.id.loaded);
			// Tell the user about this for our demo.
			//		Toast.makeText(Binding.this, R.string.local_service_connected,
			//		Toast.LENGTH_SHORT).show();
		}
		
		public void onServiceDisconnected(ComponentName className) {
			// This is called when the connection with the service has been
			// unexpectedly disconnected -- that is, its process crashed.
			// Because it is running in our same process, we should never
			// see this happen.
			//		mBoundService = null;
			//		Toast.makeText(Binding.this, R.string.local_service_disconnected,
			//		Toast.LENGTH_SHORT).show();
		}
	};
	
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.options_main, menu);
		return true;
	}
	
	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle action bar item clicks here. The action bar will
		// automatically handle clicks on the Home/Up button, so long
		// as you specify a parent activity in AndroidManifest.xml.
		int id = item.getItemId();
		
		switch(id){
			case R.id.action_start:
				startLokinet();
				return true;
			case R.id.action_stop:
				Lokinet_JNI.stopLokinet();
				return true;
		}
		
		return super.onOptionsItemSelected(item);
	}
	
}
