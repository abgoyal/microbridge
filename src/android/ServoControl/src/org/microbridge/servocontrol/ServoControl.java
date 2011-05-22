package org.microbridge.servocontrol;

import java.io.IOException;

import org.microbridge.server.Server;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;

public class ServoControl extends Activity
{

	/**
	 * Called when the activity is first created.
	 */
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		
		// Set full screen view, no title
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		
		// Create TCP server
		Server server = null;
		try
		{
			server = new Server(4567);
			server.start();
		} catch (IOException e)
		{
			Log.e("microbridge", "Unable to start TCP server", e);
			System.exit(-1);
		}
		
		JoystickView joystick = new JoystickView(this, server);
		setContentView(joystick);
		joystick.requestFocus();

	}

}