package network.loki.lokinet;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.util.Timer;
import java.util.TimerTask;


import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.IBinder;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.TextView;
import android.widget.Toast;

public class LokiNetActivity extends Activity {
	private static final String TAG = "lokinet-activity";
	private TextView textView;
	
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		// copy assets
		String conf = copyFileAsset("daemon.ini");
		
		textView = new TextView(this);
		setContentView(textView);
		if(conf == null)
		{
			Log.e(TAG, "failed to get config");
			return;
		}
		Lokinet_JNI.loadLibraries();
		Lokinet_JNI.startLokinet(conf);
	}
	
	@Override
	protected void onDestroy() {
		super.onDestroy();
		textView = null;
	}

	private CharSequence throwableToString(Throwable tr) {
		StringWriter sw = new StringWriter(8192);
		PrintWriter pw = new PrintWriter(sw);
		tr.printStackTrace(pw);
		pw.close();
		return sw.toString();
	}
	
	// private LocalService mBoundService;
	
	private ServiceConnection mConnection = new ServiceConnection() {
		public void onServiceConnected(ComponentName className, IBinder service) {
			// This is called when the connection with the service has been
			// established, giving us the service object we can use to
			// interact with the service.  Because we have bound to a explicit
			// service that we know is running in our own process, we can
			// cast its IBinder to a concrete class and directly access it.
			//		mBoundService = ((LocalService.LocalBinder)service).getService();
			
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
			case R.id.action_stop:
			Lokinet_JNI.stopLokinet();
			return true;
		}
		
		return super.onOptionsItemSelected(item);
	}
	
	/**
		* Copy the asset at the specified path to this app's data directory. If the
		* asset is a directory, its contents are also copied.
		* 
		* @param path
		* Path to asset, relative to app's assets directory.
	*/
	private void copyAsset(String path) {
		AssetManager manager = getAssets();
		Path dir = Paths.get(Environment.getExternalStorageDirectory().getAbsolutePath(), "lokinet", path);
		try {
			String[] contents = manager.list(path);
			
			// The documentation suggests that list throws an IOException, but doesn't
			// say under what conditions. It'd be nice if it did so when the path was
			// to a file. That doesn't appear to be the case. If the returned array is
			// null or has 0 length, we assume the path is to a file. This means empty
			// directories will get turned into files.
			if (contents == null || contents.length == 0)
				return;
			
			// Make the directory.
			dir.toFile().mkdirs();
			
				// Recurse on the contents.
			for (String entry : contents) {
				copyFileAsset(Paths.get(dir.toString(), entry).toString());
			}
		}
		catch(IOException ex)
		{
			copyFileAsset(path);
		}
	}
	
	/**
		* Copy the asset file specified by path to app's data directory. Assumes
		* parent directories have already been created.
		* 
		* @param path
		* Path to asset, relative to app's assets directory.
	*/
	private String copyFileAsset(String path) {
		Path p = Paths.get(Environment.getExternalStorageDirectory().getAbsolutePath(), "lokinet", path);
		try {
			p.getParent().toFile().mkdirs();
      InputStream in = getAssets().open(path);
			OutputStream out = new FileOutputStream(p.toFile());
			byte[] buffer = new byte[1024];
      int read = in.read(buffer);
      while (read != -1) {
      	out.write(buffer, 0, read);
        read = in.read(buffer);
      }
      out.close();
      in.close();
    } catch (IOException e) {
     	Log.e(TAG, "", e);
			return null;
    }
		return p.toString();
	}
}
