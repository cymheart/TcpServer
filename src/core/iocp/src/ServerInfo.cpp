#include"core/iocp/include/ServerInfo.h"
#include"core/iocp/include/Socket.h"

ServerInfo::ServerInfo()
:serverPort(0)
, name(nullptr)
, tag(0)
, localConnectPort(0)
, socketCtx(0)
, dataTransMode(MODE_PACK)
, dePacketor(nullptr)
, serverTask(nullptr)
{
}

ServerInfo::~ServerInfo()
{
	FREE(name);

	if(dePacketor != nullptr)	
		DEBUG("%s:%I64u,减少引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef() - 1);

	RELEASE_REF(dePacketor);
}

void ServerInfo::Copy(ServerInfo& obj)
{
	SetName(obj.name);

	memcpy(serverIP, obj.serverIP, 16);
	serverPort = obj.serverPort;
	localConnectPort = obj.localConnectPort;
	tag = obj.tag;


	SetDePacketor(obj.dePacketor);
}


inline void ServerInfo::SetDePacketor(DePacketor*  _dePacketor)
{
	if (dePacketor == _dePacketor)
		return;

	if (dePacketor != nullptr)
		DEBUG("%s:%I64u,减少引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef() - 1);

	RELEASE_REF(dePacketor);
	dePacketor = _dePacketor;	

	if (dePacketor != nullptr) {
		dePacketor->addRef();
		DEBUG("%s:%I64u,增加引用为:%d", dePacketor->GetName().c_str(), (uint64_t)dePacketor, dePacketor->getRef());
	}
}

void ServerInfo::SetName(char* name)
{
	FREE(this->name);

	if (name == NULL)
		return;

	size_t len = strlen(name);
	char* newName = (char*)MALLOC((len + 1) * sizeof(char));
	strcpy(newName, name);
	this->name = newName;
}