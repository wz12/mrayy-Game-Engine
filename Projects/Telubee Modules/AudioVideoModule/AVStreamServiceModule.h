

#ifndef __AVStreamServiceModule__
#define __AVStreamServiceModule__

#include "IServiceModule.h"
#undef StartService

namespace mray
{
namespace TBee
{
	
	class AVStreamServiceModuleImpl;
class AVStreamServiceModule:public IServiceModule
{
public:
	static const std::string ModuleName;

	DECLARE_RTTI
protected:
	AVStreamServiceModuleImpl* m_impl;
public:
	AVStreamServiceModule();
	virtual ~AVStreamServiceModule();

	virtual std::string GetServiceName() ;
	virtual EServiceStatus GetServiceStatus() ;

	virtual void InitService(ServiceContext* context);
	virtual EServiceStatus StartService(ServiceContext* context);
	virtual bool StopService() ;
	virtual void DestroyService() ;

	virtual void Update(float dt) ;
	virtual void Render(ServiceRenderContext* contex) ;
	virtual void DebugRender(ServiceRenderContext* contex) ;

	virtual bool LoadServiceSettings(xml::XMLElement* e) ;
	virtual void ExportServiceSettings(xml::XMLElement* e) ;
};

}
}


#endif
