// For conditions of distribution and use, see copyright notice in license.txt

#include <boost/test/unit_test.hpp>

#include "CoreStdIncludes.h"
#include "Core.h"
#include "Foundation.h"
#include "ServiceInterfaces.h"

//! Unit test for framework
BOOST_AUTO_TEST_SUITE(test_suite_support_modules)


struct TestA
{
    Foundation::Console::CommandResult TestCallbackSuccess(const Core::StringVector &params)
    {
        BOOST_CHECK_EQUAL(params[0], "paramA");
        BOOST_CHECK_EQUAL(params[1], "paramB");

        //Foundation::Console::CommandResult result = {true, "Success"};
        return Foundation::Console::ResultSuccess("Success");
    }
    Foundation::Console::CommandResult TestCallbackFailure(const Core::StringVector &params)
    {
        BOOST_CHECK_EQUAL(params.size(), 0);

        return Foundation::Console::ResultFailure();
    }
};

BOOST_AUTO_TEST_CASE( support_modules_console )
{
    Foundation::Framework fw;
    
    fw.GetModuleManager()->LoadModuleByName("SupportModules", "Console");
    BOOST_CHECK (fw.GetModuleManager()->HasModule(Foundation::Module::MT_Console));
    
    Foundation::Console::ConsoleServiceInterface *console = fw.GetService<Foundation::Console::ConsoleServiceInterface>
        (Foundation::Service::ST_Console);

    TestA test_class;
    Foundation::Console::Command commandA = {"Test_CommandA", "Test command Success", Foundation::Console::Bind(&test_class, &TestA::TestCallbackSuccess) };
    console->RegisterCommand(commandA);
    Foundation::Console::Command commandB = {"Test_CommandB", "Test command Failure", Foundation::Console::Bind(&test_class, &TestA::TestCallbackFailure) };
    console->RegisterCommand(commandB);
    

    Foundation::Console::CommandResult result = console->ExecuteCommand("Test_CommandA (paramA, paramB )");
    BOOST_CHECK_EQUAL (result.success_, true);
    BOOST_CHECK_EQUAL (result.why_, "Success");

    result = console->ExecuteCommand("Test_CommandB");
    BOOST_CHECK_EQUAL (result.success_, false);
    BOOST_CHECK_EQUAL (result.why_.size(), 0);

    

    fw.GetModuleManager()->UninitializeModules();
    fw.GetModuleManager()->UnloadModules();
}


BOOST_AUTO_TEST_SUITE_END()

