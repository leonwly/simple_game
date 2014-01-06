#include <iostream>

#include "net.hpp"
#include "base.hpp"
#include "rule.hpp"
#include "db.hpp"

#include "PlayerModule.hpp"

#include <boost/thread.hpp>
#include <boost/function.hpp>

using namespace std;




NETWORK_BEGIN
	using namespace sql;
	typedef void (*FuncHandle)(uint64 sessionId, IServer *server, ByteBuffer& data);
	
	FuncHandle g_handles[RULE_NUM] = {0};
	Connection *g_conn = NULL;

	void HANDLE_L2D_PLAYER_INFO(uint64 sessionId, IServer *server, ByteBuffer& data)
	{
		PlayerInfo *info = NULL;
		
		uint8 tmpType;
		uint64 c_sessionId;
		
		data >> tmpType;
		data >> c_sessionId;

		if (tmpType == DB_PLAYER_NAME)
		{
			char name[MAX_NAME + 1] = {0};
			data.read((uint8*)name, MAX_NAME);
			info = PlayerModule::instance().findPlayerInfoByName(name);
		}
		else if (tmpType == DB_PLAYER_UID)
		{
			uint32 userId = 0;
			data >> userId;
			info = PlayerModule::instance().findPlayerInfoByUid(userId);
		}
		
		ByteBuffer buffer;
		buffer.append((uint16)D2L_PLAYER_INFO);
		buffer << c_sessionId;

		if (info == NULL)
		{
			buffer.append((uint8)DB_ERR);
		}
		else
		{
			buffer.append((uint8)DB_SUCC);
			buffer.append((uint8*)info, sizeof(PlayerInfo));
		}

		server->sendTo(sessionId, buffer);
	}
	

class DBServer : public IServerHandle, public IClientHandle
{
public:
	DBServer()
		: m_pMaster(0)
		, m_pServer(0)
	{

	}

	~DBServer()
	{

	}

	void run()
	{

		Config cfg;
		cfg.loadIni("option.ini");
		
		std::string master_host = cfg.getString("master_host");
		int master_port = cfg.getInt("master_port");

		std::string db_host = cfg.getString("db_host");
		std::string db_user = cfg.getString("db_user");
		std::string db_pwd = cfg.getString("db_pwd");

		int listener_port = cfg.getInt("listener_port");

		int request_threadnum = cfg.getInt("request_threadnum");
		int response_threadnum = cfg.getInt("response_threadnum");

		m_pMaster = NetLib::instance().createClient(master_host.c_str(), master_port);
		m_pServer = NetLib::instance().createServer(listener_port);

		m_pServer->bindHandle(this);
		m_pMaster->bindHandle(this);

		g_handles[L2D_PLAYER_INFO] = &HANDLE_L2D_PLAYER_INFO;


		g_conn = DbLib::instance().connect(db_host.c_str(), db_user.c_str(), db_pwd.c_str());
		g_conn->setSchema("net_db");

		PlayerModule::instance().bind(g_conn);

		NetLib::instance().run();
	}
	// server
	void onReciveServerHandle(uint64 serverId, uint64 sessionId, ByteBuffer& data)
	{
		data.read_skip(4);
		uint16 cmd = data.read<uint16>();
		FuncHandle handle = g_handles[cmd];
		if (handle)
			handle(sessionId, m_pServer, data);

		//sLog.outMessage("[GateServer::onReciveServerHandle] cmd: %d", cmd);
	}

	void onErrorServerHandle(IServer *server, const boost::system::error_code& error)
	{
		uint64 serverId = server->getId();
		uint16 port = server->getPort();
		NetLib::instance().destroyServer(serverId);
		sLog.outError("server error serverId:%ld port: %d error:%s \n", serverId, port, error.message().c_str());
	}

	// client
	void onReciveClientHandle(uint64 clientId, ByteBuffer& data)
	{
		uint16 cmd = data.read<uint16>();
		sLog.outMessage("[GateServer::onReciveServerHandle] cmd: %d", cmd);
	}

	void onErrorClientHandle(IClient *client, const boost::system::error_code& error)
	{
		uint64 clientId = client->getId();
		std::string host = client->getHost();
		uint16 port = client->getPort();

		NetLib::instance().destroyClient(clientId);
		//sLog.outError("client error clientId:%ld host:%s port:%d error:%s \n", clientId, host.c_str(), port, error.message().c_str());
		sLog.outError("client:%d %s", clientId, error.message().c_str());
	}

protected:
	IServer *m_pServer;
	IClient *m_pMaster;
};

NETWORK_END

	int main(int argc, char** argv)
{
	USE_NETWORK;
	DBServer app;
	app.run();
	return 0;
}