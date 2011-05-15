package org.tudelft.microbridge.server;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.Socket;

public class Client
{
	
	private String name;
	private Socket socket;
	
	private final Server server;
	
	private final BufferedReader reader;
	private final BufferedWriter writer;
	
	private Thread communicationThread;
	
	private boolean keepAlive = true;
	
	public Client(Server server, Socket socket) throws IOException
	{
		this.server = server;
		this.socket = socket;
		socket.setKeepAlive(true);
		
		this.reader = new BufferedReader(new InputStreamReader(this.socket.getInputStream()));
		this.writer = new BufferedWriter(new OutputStreamWriter(this.socket.getOutputStream()));

		startCommunicationThread();
	}	
	
	public void startCommunicationThread()
	{
		(communicationThread = new Thread() {
			public void run()
			{
				while (keepAlive)
				{
					try
					{
						String line = reader.readLine();
						
						if (line==null)
							keepAlive = false;
	
						processCommand(line);
						
					} catch (IOException e)
					{
						throw new RuntimeException(e);
					}
				}
				
				// Cliente exited, notify parent server
				server.disconnectClient(Client.this);
			}
		}).start();
	}
	
	public void processCommand(String command)
	{
		
	}
	
	public void send(String command) throws IOException
	{
		writer.write(command + "\n");
		writer.flush();
	}

}
