// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"

#include "TestModuleB.h"
#include <Poco/ClassLibrary.h>
#include "Foundation.h"


namespace Test
{
    TestModuleB::TestModuleB() : ModuleInterface_Impl("TestModuleB")
    {
    }

    TestModuleB::~TestModuleB()
    {
    }

    // virtual
    void TestModuleB::Load()
    {
        LOG("Module " + Name() + " loaded.");
    }

    // virtual
    void TestModuleB::Unload()
    {
        LOG("Module " + Name() + " unloaded.");
    }

    // virtual
    void TestModuleB::Initialize(Foundation::Framework *framework)
    {
        assert(framework != NULL);
        framework_ = framework;
        
        LOG("Module " + Name() + " initialized.");
    }

    // virtual 
    void TestModuleB::Uninitialize(Foundation::Framework *framework)
    {
        assert(framework_ != NULL);
        framework_ = NULL;
        
        LOG("Module " + Name() + " uninitialized.");
    }

    // virtual
    void TestModuleB::Update()
    {
        Foundation::TestServiceInterface *test_service = framework_->GetServiceManager()->GetService<Foundation::TestServiceInterface>(Foundation::Service::ST_Test);
        assert (test_service != NULL);
        assert (test_service->Test());
    }
}

