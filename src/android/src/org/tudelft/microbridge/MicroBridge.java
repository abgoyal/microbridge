package org.tudelft.microbridge;

import java.io.IOException;

import org.tudelft.microbridge.server.Client;
import org.tudelft.microbridge.server.Server;
import org.tudelft.microbridge.server.ServerListener;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Toast;
import android.widget.SeekBar.OnSeekBarChangeListener;

public class MicroBridge extends Activity implements ServerListener
{
	
	private Handler handler = new Handler();
	private final Server server;
	private Button connectionButton;
	
	public MicroBridge()
	{
		server = new Server();
		server.addListener(this);
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		ScrollView scrollView = new ScrollView(this);
		LinearLayout layout = new LinearLayout(this);
		layout.setOrientation(LinearLayout.VERTICAL);
		scrollView.addView(layout);
		this.setContentView(scrollView);
		
		connectionButton = new Button(this);
		layout.addView(connectionButton);
		setConnectionButtonStart();
		
		SeekBar seekBar = new SeekBar(this);
		layout.addView(seekBar);
		
		seekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener()
		{
			public void onStopTrackingTouch(SeekBar arg0)
			{
			}
			
			public void onStartTrackingTouch(SeekBar arg0)
			{
			}
			
			public void onProgressChanged(SeekBar arg0, int arg1, boolean arg2)
			{
				try
				{
					if (arg1>99) arg1 = 99;
					server.send(String.format("S%02d", arg1));
					
				} catch (IOException e)
				{
					Log.e("SEEK", e.toString());
				}
			}
		});
		
	}

	private void setConnectionButtonStart()
	{
		connectionButton.setText("Start server");
		
		connectionButton.setOnClickListener(new View.OnClickListener()
		{
			public void onClick(View v)
			{
				try
				{
					server.start();
				} catch (IOException e)
				{
					Toast toast = Toast.makeText(MicroBridge.this, "Unable to start server: " + e.toString(), Toast.LENGTH_SHORT);
					toast.show();
				}
			}
		});
	}

	private void setConnectionButtonStop()
	{
		connectionButton.setText("Stop server");
		
		connectionButton.setOnClickListener(new View.OnClickListener()
		{
			public void onClick(View v)
			{
				server.stop();
			}
		});
	}

	public void onClientConnect(Server server, Client client)
	{
		handler.post(new Runnable() {
			public void run()
			{
				Toast toast = Toast.makeText(MicroBridge.this, "Client connected", Toast.LENGTH_SHORT);
				toast.show();
			}
		});
	}

	public void onClientDisconnect(Server server, Client client)
	{
		handler.post(new Runnable() {
			public void run()
			{
				Toast toast = Toast.makeText(MicroBridge.this, "Client disconnected", Toast.LENGTH_SHORT);
				toast.show();
			}
		});
	}

	public void onServerStarted(Server server)
	{
		Toast toast = Toast.makeText(MicroBridge.this, "Server started on port " + server.getPort(), Toast.LENGTH_SHORT);
		toast.show();
		
		setConnectionButtonStop();
	}

	public void onServerStopped(Server server)
	{
		Toast toast = Toast.makeText(MicroBridge.this, "Server stopped", Toast.LENGTH_SHORT);
		toast.show();
		
		setConnectionButtonStart();
	}
	
}