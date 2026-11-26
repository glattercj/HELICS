/*
Copyright (c) 2017-2026,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Energy
Innovation LLC.  See the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/

#include "../application_api/testFixtures.hpp"
#include "helics/common/JsonProcessingFunctions.hpp"
#include "helics/core/core-exceptions.hpp"

#include "gtest/gtest.h"
#include <complex>

/** these test cases test out the value converters
 */
#include "helics/application_api.hpp"
#include "helics/network/test/TestComms.h"
#include "helics/network/test/TestCore.h"

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <thread>

#define CORE_TYPE_TO_TEST helics::CoreType::TEST

// the state a broker reports for one of the cores it knows
static std::string coreState(helics::Broker& brk, const std::string& coreName)
{
    auto state = helics::fileops::loadJsonStr(brk.query(brk.getIdentifier(), "current_state"));
    for (const auto& core : state["cores"]) {
        if (core["attributes"]["name"] == coreName) {
            return core["state"].get<std::string>();
        }
    }
    return "missing";
}

TEST(broker_timeout, configured_timeout_is_not_shortened)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=10s --tick=500ms");
    ASSERT_TRUE(brk->connect());

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreName = "timeout_units_core";
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier() + " --timeout=20s";
    helics::Federate fed("timeout_units_federate", fedInfo);
    fed.enterExecutingMode();

    auto core = std::dynamic_pointer_cast<helics::testcore::TestCore>(fed.getCorePointer());
    ASSERT_TRUE(core);
    auto comms = core->getCommsObjectPointer();
    comms->pauseComms(true);
    // Several broker heartbeats may pass without a reply, but not its 10 s timeout.
    EXPECT_FALSE(brk->waitForDisconnect(std::chrono::seconds(3)));
    comms->pauseComms(false);

    EXPECT_TRUE(brk->isConnected());
    EXPECT_TRUE(core->isConnected());
    if (brk->isConnected() && core->isConnected()) {
        EXPECT_EQ(fed.requestTime(1.0), helics::Time(1.0));
    }
    fed.finalize();
    brk->disconnect();
}

TEST(broker_timeout, core_fail_timeout)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=100ms --tick 30ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreName = "c1";

    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreName = "c2";
    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    Fed1->finalize();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    bool val = brk->waitForDisconnect(std::chrono::milliseconds(1000));
    if (!val) {
        val = brk->waitForDisconnect(std::chrono::milliseconds(2000));
    }
    if (!val) {
        brk->waitForDisconnect(std::chrono::milliseconds(1000));
    }
    EXPECT_TRUE(val);
    core->disconnect();
    Fed2->finalize();
}
// this test is exactly like the previous one except the core was specified with no_ping so it won't
// fail
TEST(broker_timeout, core_fail_timeout_no_ping_ci_skip)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=200ms --tick 50ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreName = "c1";

    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreName = "c2";
    fedInfo.coreInitString = "--slow_responding";

    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    Fed1->finalize();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    bool val = brk->waitForDisconnect(std::chrono::milliseconds(2000));
    EXPECT_FALSE(val);
    brk->disconnect();

    core->disconnect();
    Fed1->finalize();
    Fed2->finalize();
}

// this test is exactly like the previous one except the core was specified with debugging so it
// won't fail
TEST(broker_timeout, core_fail_debugging_ci_skip)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=200ms --tick 50ms ");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreName = "c1";

    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreName = "c2";
    fedInfo.coreInitString = "--debugging";

    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    Fed1->finalize();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    bool val = brk->waitForDisconnect(std::chrono::milliseconds(2000));
    EXPECT_FALSE(val);
    brk->disconnect();

    core->disconnect();
    Fed1->finalize();
    Fed2->finalize();
}

// this test is similar in concept to the previous two but using --disable_timer flag
TEST(broker_timeout, core_fail_timeout_no_timer_ci_skip)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--timeout=200ms --tick 50ms --disable_timer");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreName = "c1";

    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreName = "c2";

    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    Fed1->finalize();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    bool val = brk->waitForDisconnect(std::chrono::milliseconds(2000));
    EXPECT_FALSE(val);
    brk->disconnect();

    core->disconnect();
    Fed1->finalize();
    Fed2->finalize();
}

// a broker that never sees an idle tick must still notice a core that stopped responding
TEST(broker_timeout, core_fail_timeout_with_traffic)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=1s --tick=500ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreName = "ctraffic1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreName = "ctraffic2";
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    fedInfo.coreName = "ctraffic3";
    auto Fed3 = std::make_shared<helics::Federate>("test3", fedInfo);

    auto& pub1 = Fed1->registerGlobalPublication<double>("traffic1");
    auto& pub2 = Fed2->registerGlobalPublication<double>("traffic2");
    Fed1->registerSubscription("traffic2");
    Fed2->registerSubscription("traffic1");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingModeAsync();
    Fed3->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed2->enterExecutingModeComplete();

    // keep values and time messages flowing through the broker in every tick period
    std::atomic<bool> stop{false};
    auto traffic = [&stop](helics::ValueFederate* fed, helics::Publication* pub) {
        try {
            while (!stop.load()) {
                pub->publish(fed->getCurrentTime());
                fed->requestNextStep();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        catch (const helics::HelicsException&) {
            // the federation errors once the lost core is detected
        }
    };
    std::thread traffic1(traffic, Fed1.get(), &pub1);
    std::thread traffic2(traffic, Fed2.get(), &pub2);

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    auto core = Fed3->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    bool val = brk->waitForDisconnect(std::chrono::milliseconds(6000));
    EXPECT_TRUE(val);
    stop.store(true);
    if (!val) {
        brk->disconnect();
    }
    traffic1.join();
    traffic2.join();
    core->disconnect();
    Fed1->finalize();
    Fed2->finalize();
    Fed3->finalize();
}

// a broker that has handled one lost core must still notice the next one
TEST(broker_timeout, core_fail_timeout_second_core)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=200ms --tick=50ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreName = "csecond1";
    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreName = "csecond2";
    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    fedInfo.coreName = "csecond3";
    auto Fed3 = std::make_shared<helics::Federate>("test3", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingModeAsync();
    Fed3->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed2->enterExecutingModeComplete();

    // a clean disconnect first, so the lost cores are handled as disconnects
    Fed1->finalize();

    auto core2 = Fed2->getCorePointer();
    auto tcr2 = std::dynamic_pointer_cast<helics::testcore::TestCore>(core2);
    EXPECT_TRUE(tcr2);
    tcr2->getCommsObjectPointer()->haltComms();
    tcr2.reset();

    // let the broker detect and drop the first lost core
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    EXPECT_TRUE(brk->isConnected());

    auto core3 = Fed3->getCorePointer();
    auto tcr3 = std::dynamic_pointer_cast<helics::testcore::TestCore>(core3);
    EXPECT_TRUE(tcr3);
    tcr3->getCommsObjectPointer()->haltComms();
    tcr3.reset();

    bool val = brk->waitForDisconnect(std::chrono::milliseconds(3000));
    EXPECT_TRUE(val);
    if (!val) {
        brk->disconnect();
    }
    core2->disconnect();
    core3->disconnect();
    Fed2->finalize();
    Fed3->finalize();
}

// a reentrant federate that comes back on a new core before the broker has noticed that its old
// core is gone replaces the old connection, and the later timeout is not an error
TEST(broker_timeout, reentrant_rejoin_before_core_fail)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=2s --tick=200ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreName = "cearly1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreName = "cearly2";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in1 = Fed1->registerSubscription("early2");
    Fed2->registerGlobalPublication<double>("early2");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    // rejoin right away, well inside the broker timeout
    fedInfo.coreName = "cearly3";
    fedInfo.coreInitString = "--dynamic --broker=" + brk->getIdentifier();
    std::unique_ptr<helics::ValueFederate> Fed2Redo;
    EXPECT_NO_THROW(Fed2Redo = std::make_unique<helics::ValueFederate>("test2", fedInfo));
    if (Fed2Redo) {
        auto& pub2redo = Fed2Redo->registerGlobalPublication<double>("early2");
        EXPECT_NO_THROW(Fed2Redo->enterExecutingMode());
        if (Fed2Redo->getCurrentMode() == helics::Federate::Modes::EXECUTING) {
            pub2redo.publish(-7.25);
            Fed2Redo->requestTimeAsync(5.0);
            helics::Time granted{helics::timeZero};
            EXPECT_NO_THROW(granted = Fed1->requestTime(5.0));
            EXPECT_GT(granted, helics::timeZero);
            EXPECT_DOUBLE_EQ(in1.getDouble(), -7.25);
            Fed2Redo->requestTimeComplete();
        }
        Fed2Redo->finalize();
    }

    // wait until the old core would have timed out
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    EXPECT_TRUE(brk->isConnected());
    EXPECT_NO_THROW(Fed1->requestTime(10.0));
    Fed1->finalize();
    core->disconnect();
    brk->disconnect();
}

// a second process with the same reentrant name is refused while the first one is still connected
TEST(broker_timeout, reentrant_rejoin_while_connected)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=2s --tick=200ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreName = "clive1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreName = "clive2";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    auto& in1 = Fed1->registerSubscription("live2");
    auto& pub2 = Fed2->registerGlobalPublication<double>("live2");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    fedInfo.coreName = "clive3";
    fedInfo.coreInitString = "--dynamic --broker=" + brk->getIdentifier();
    EXPECT_THROW(helics::ValueFederate("test2", fedInfo), helics::RegistrationFailure);

    // the first one is untouched
    pub2.publish(3.5);
    Fed2->requestTimeAsync(2.0);
    EXPECT_GT(Fed1->requestTime(2.0), helics::timeZero);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 3.5);
    EXPECT_EQ(Fed2->requestTimeComplete(), 2.0);
    Fed2->finalize();
    Fed1->finalize();
    brk->disconnect();
}

// a reentrant federate under a subbroker registers again through that subbroker on a new core
// before its old core is found to be lost
TEST(broker_timeout, reentrant_rejoin_before_core_fail_subbroker)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=30s --tick=200ms");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=2s --tick=200ms --broker=" +
                                                 brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "csubearly1";
    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.coreName = "csubearly2";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    fedInfo.coreName = "csubearly3";
    fedInfo.coreInitString = "--dynamic --broker=" + sub->getIdentifier();
    std::unique_ptr<helics::Federate> Fed2Redo;
    EXPECT_NO_THROW(Fed2Redo = std::make_unique<helics::Federate>("test2", fedInfo));
    if (Fed2Redo) {
        EXPECT_NO_THROW(Fed2Redo->enterExecutingMode());
    }
    // the subbroker dropped the old core and told the root
    EXPECT_EQ(coreState(*sub, "csubearly2"), "disconnected");
    EXPECT_EQ(coreState(*brk, "csubearly2"), "disconnected");
    EXPECT_TRUE(brk->isConnected());
    EXPECT_NO_THROW(Fed1->requestTime(2.0));
    if (Fed2Redo) {
        Fed2Redo->finalize();
    }
    Fed1->finalize();
    core->disconnect();
    sub->disconnect();
    brk->disconnect();
}

TEST(broker_timeout, core_fail_error)
{
    auto brk = helics::BrokerFactory::create(helics::CoreType::TEST, "--timeout=200ms --tick 50ms");
    brk->connect();

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.coreName = "c3";

    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreName = "c4";
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    Fed1->registerPublication("p1", "double");
    Fed2->registerSubscription("test1/p1");
    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    auto core = Fed1->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    EXPECT_THROW(Fed2->requestTime(1.0), helics::HelicsException);
    core->disconnect();
    Fed2->finalize();
    Fed1->finalize();
}

// losing a core whose federates are all reentrant is treated as a disconnect so they can rejoin
TEST(broker_timeout, reentrant_core_fail_rejoin)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=1s --tick=500ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreName = "crent1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreName = "crent2";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in1 = Fed1->registerSubscription("pub2");
    Fed2->registerGlobalPublication<double>("pub2");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);

    auto cms = tcr->getCommsObjectPointer();
    cms->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    // give the broker time to detect the lost core
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    EXPECT_TRUE(brk->isConnected());
    EXPECT_NE(Fed1->getCurrentMode(), helics::Federate::Modes::ERROR_STATE);

    fedInfo.coreName = "crent3";
    fedInfo.coreInitString = "--dynamic --broker=" + brk->getIdentifier();
    std::unique_ptr<helics::ValueFederate> Fed2Redo;
    EXPECT_NO_THROW(Fed2Redo = std::make_unique<helics::ValueFederate>("test2", fedInfo));
    if (Fed2Redo && brk->isConnected()) {
        auto& pub2redo = Fed2Redo->registerGlobalPublication<double>("pub2");
        Fed2Redo->enterExecutingMode();
        pub2redo.publish(-19.45);
        Fed2Redo->requestTimeAsync(5.0);
        helics::Time granted{helics::timeZero};
        EXPECT_NO_THROW(granted = Fed1->requestTime(5.0));
        EXPECT_GT(granted, helics::timeZero);
        EXPECT_DOUBLE_EQ(in1.getDouble(), -19.45);
        Fed2Redo->requestTimeComplete();
        Fed2Redo->finalize();
    }
    Fed1->finalize();
    core->disconnect();
    brk->disconnect();
}

#ifdef HELICS_ENABLE_ZMQ_CORE
TEST(broker_timeout, maintain_connection_ci_skip)
{
    auto brk = helics::BrokerFactory::create(helics::CoreType::ZMQ, "--timeout=100ms");
    brk->connect();

    helics::FederateInfo fedInfo(helics::CoreType::ZMQ);
    fedInfo.coreName = "c3";
    fedInfo.setProperty(HELICS_PROPERTY_TIME_DELTA, 0.01);
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    Fed1->registerGlobalPublication<std::string>("pub1");
    auto& sub1 = Fed1->registerSubscription("pub1");
    sub1.setDefault(std::string("String1"));

    Fed1->enterExecutingMode();
    int counter = 50;
    while (counter > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        --counter;
    }
    EXPECT_TRUE(brk->isConnected());
    EXPECT_TRUE(Fed1->getCorePointer()->isConnected());
    Fed1->finalize();
    brk->waitForDisconnect();
}
#endif

TEST(broker_timeout, max_duration)
{
    auto brk = helics::BrokerFactory::create(helics::CoreType::TEST,
                                             "--maxcosimduration=300ms --tick 50ms");
    brk->connect();
    // the query is just to force the thread to be operating
    auto str = brk->query(brk->getIdentifier(), "exists");
    EXPECT_EQ(str, "true");

    auto res = brk->waitForDisconnect(std::chrono::milliseconds(900));
    if (!res) {
        // this may get to this condition in some slower CI test systems
        res = brk->waitForDisconnect(std::chrono::milliseconds(500));
    }
    EXPECT_TRUE(res);
    if (!res) {
        brk->disconnect();
    }
}

TEST(broker_timeout, max_duration_core)
{
    auto brk = helics::BrokerFactory::create(helics::CoreType::TEST,
                                             "--maxcosimduration=300ms --tick 50ms");
    brk->connect();
    // the query is just to force the thread to be operating
    auto str = brk->query(brk->getIdentifier(), "exists");
    EXPECT_EQ(str, "true");

    auto core = helics::CoreFactory::create(CORE_TYPE_TO_TEST, "--broker=" + brk->getIdentifier());
    EXPECT_TRUE(core->connect());
    auto res = core->waitForDisconnect(std::chrono::milliseconds(600));
    if (!res) {
        res = core->waitForDisconnect(std::chrono::milliseconds(400));
    }
    EXPECT_TRUE(res);
    if (!res) {
        brk->disconnect();
    }
}
