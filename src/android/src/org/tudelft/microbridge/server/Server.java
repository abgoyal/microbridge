package org.tudelft.microbridge.server;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.TreeSet;

public class Server
{
	
	private ServerSocket serverSocket = null;
	private ArrayList<Client> clients = new ArrayList<Client>();
	private TreeSet<ServerListener> listeners = new TreeSet<ServerListener>();
	
	private boolean keepAlive = true;
	
	private int port = 4567;
	
	public Server()
	{
	}
	
	public Server(int port)
	{
		this();
		this.port = port;
	}
	
	public int getPort()
	{
		return port;
	}
	
	public void start() throws IOException
	{
		keepAlive = true;
		serverSocket = new ServerSocket(port);
		
		(new Thread(){
			public void run()
			{
				Socket socket;
				try
				{
					while (keepAlive)
					{
						
						try {

							socket = serverSocket.accept();

							// Create Client object.
							Client client = new Client(Server.this, socket);
							clients.add(client);
							
							// Notify listeners.
							for (ServerListener listener : listeners)
								listener.onClientConnect(Server.this, client);
						
						} catch (SocketException ex)
						{
							// A SocketException is thrown when the stop method calls 'close' on the
							// serverSocket object. This means we should break out of the connection
							// accept loop.
							keepAlive = false;
						}
					
					}
					
				} catch (IOException e)
				{
					// TODO
				}
			}
		}).start();
		
		// Notify listeners.
		for (ServerListener listener : listeners)
			listener.onServerStarted(this);
		
	}
	
	public void stop()
	{
		if (serverSocket!=null)
			try
			{
				serverSocket.close();
			} catch (IOException e)
			{
				// TODO
			}
		
		// Notify listeners.
		for (ServerListener listener : listeners)
			listener.onServerStopped(this);
		
	}
	
	protected void disconnectClient(Client client)
	{
		this.clients.remove(client);
		
		for (ServerListener listener : listeners)
			listener.onClientDisconnect(Server.this, client);
	}
	
	public void addListener(ServerListener listener)
	{
		this.listeners.add(listener);
	}
	
	public void removeListener(ServerListener listener)
	{
		this.listeners.remove(listener);
	}
	
	public void send(String command) throws IOException
	{
		for (Client client : clients)
			client.send(command);
	}

}
