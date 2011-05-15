package org.tudelft.microbridge.server;

public interface ServerListener
{

	public void onServerStarted(Server server);
	public void onServerStopped(Server server);
	public void onClientConnect(Server server, Client client);
	public void onClientDisconnect(Server server, Client client);

}
