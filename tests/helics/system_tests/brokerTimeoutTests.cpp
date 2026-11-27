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
#include "helics/core/ActionMessage.hpp"
#include "helics/core/CoreBroker.hpp"
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

// the old instance leaves cleanly, but its disconnect reaches the broker only after the new
// registration; the core still answers the ping, and the registration goes through
TEST(broker_timeout, reentrant_rejoin_disconnect_after_registration)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=2s --tick=200ms");
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreName = "cslow1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    // on the same core, which stays up after it leaves
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in1 = Fed1->registerSubscription("slow2");
    Fed2->registerGlobalPublication<double>("slow2");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(Fed2->getCorePointer());
    ASSERT_TRUE(tcr);
    auto comms = tcr->getCommsObjectPointer();
    // hold back what the core sends so its disconnect arrives after the new registration
    comms->pauseComms(true);
    Fed2->finalizeAsync();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fedInfo.coreName = "cslow2";
    fedInfo.coreInitString = "--dynamic --broker=" + brk->getIdentifier();
    auto redo = std::async(std::launch::async, [&fedInfo]() {
        return std::make_unique<helics::ValueFederate>("test2", fedInfo);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    comms->pauseComms(false);

    std::unique_ptr<helics::ValueFederate> Fed2Redo;
    EXPECT_NO_THROW(Fed2Redo = redo.get());
    Fed2->finalizeComplete();
    if (Fed2Redo) {
        auto& pub2redo = Fed2Redo->registerGlobalPublication<double>("slow2");
        EXPECT_NO_THROW(Fed2Redo->enterExecutingMode());
        if (Fed2Redo->getCurrentMode() == helics::Federate::Modes::EXECUTING) {
            pub2redo.publish(4.5);
            Fed2Redo->requestTimeAsync(5.0);
            EXPECT_GT(Fed1->requestTime(5.0), helics::timeZero);
            EXPECT_DOUBLE_EQ(in1.getDouble(), 4.5);
            Fed2Redo->requestTimeComplete();
        }
        Fed2Redo->finalize();
    }
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
static void loseReentrantCoreAndRejoin(const std::string& tag, const std::string& brokerArgs)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=1s --tick=500ms " + brokerArgs);
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "crent1" + tag;
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreName = "crent2" + tag;
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

    fedInfo.coreName = "crent3" + tag;
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

TEST(broker_timeout, reentrant_core_fail_rejoin)
{
    loseReentrantCoreAndRejoin("", "");
}

// the lost federate is disconnected for its dependents too, and still rejoins
TEST(broker_timeout, reentrant_core_fail_rejoin_lost_connection_flag)
{
    loseReentrantCoreAndRejoin("flag", "--disconnect_on_lost_connection");
}

// lose the core of one federate under a subbroker that also holds a federate on another core;
// subArgs sets the subbroker timers: with its default 30 s timeout only the root can see the loss
// within the test, with a shorter one than the root the subbroker sees it first
static void
    loseCoreUnderSubbroker(const std::string& tag, const std::string& subArgs, bool reentrant)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=1s --tick=200ms");
    brk->connect();
    auto sub =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                      "--dynamic --broker=" + brk->getIdentifier() + " " + subArgs);
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "csubroot" + tag;
    auto Fed1 = std::make_shared<helics::Federate>("test1", fedInfo);

    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.coreName = "csubother" + tag;
    auto Fed3 = std::make_shared<helics::Federate>("test3", fedInfo);

    fedInfo.coreName = "csublost" + tag;
    if (reentrant) {
        fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    }
    auto Fed2 = std::make_shared<helics::Federate>("test2", fedInfo);

    Fed1->enterExecutingModeAsync();
    Fed3->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed3->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    if (reentrant) {
        // give the brokers time to detect the lost core
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        EXPECT_TRUE(brk->isConnected());
        EXPECT_TRUE(sub->isConnected());
        EXPECT_EQ(coreState(*brk, "csublost" + tag), "disconnected");
        EXPECT_EQ(coreState(*sub, "csublost" + tag), "disconnected");
    } else {
        EXPECT_TRUE(brk->waitForDisconnect(std::chrono::milliseconds(5000)));
    }
    Fed1->finalize();
    Fed3->finalize();
    core->disconnect();
    sub->disconnect();
    brk->disconnect();
}

// a lost core whose federates are all reentrant is a disconnect also under a subbroker, even
// though another federate of that subbroker is not reentrant
TEST(broker_timeout, reentrant_core_fail_subbroker)
{
    // only the root can see the loss
    loseCoreUnderSubbroker("rt", "", true);
}

TEST(broker_timeout, reentrant_core_fail_subbroker_detects)
{
    // the subbroker sees the loss first
    loseCoreUnderSubbroker("st", "--timeout=300ms --tick=100ms", true);
}

// losing a core with a federate that is not reentrant under a subbroker still errors
TEST(broker_timeout, core_fail_subbroker_error)
{
    loseCoreUnderSubbroker("re", "", false);
    loseCoreUnderSubbroker("se", "--timeout=300ms --tick=100ms", false);
}

// the state a broker reports for a federate
static std::string federateState(helics::Broker& broker, std::string_view name)
{
    auto state =
        helics::fileops::loadJsonStr(broker.query(broker.getIdentifier(), "current_state"));
    for (const auto& fed : state["federates"]) {
        if (fed["attributes"]["name"].get<std::string>() == name) {
            return fed["state"].get<std::string>();
        }
    }
    return std::string{};
}

// a reentrant federate under a subbroker is lost and then rejoins through the same subbroker
TEST(broker_timeout, reentrant_fed_lost_under_subbroker)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=3s --tick 100ms -f 3");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=3s --tick 100ms --broker=" +
                                                 brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "lost_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);
    // a federate that leaves cleanly so the lost one is disconnected instead of erroring
    fedInfo.coreName = "lost_c2";
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    fedInfo.coreName = "lost_c3";
    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed3 = std::make_shared<helics::ValueFederate>("fed3", fedInfo);

    auto& pub1 = Fed1->registerGlobalPublication<double>("pub1");
    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in3 = Fed1->registerSubscription("pub3");
    Fed3->registerSubscription("pub1");
    Fed3->registerGlobalPublication<double>("pub3");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingModeAsync();
    Fed3->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed2->enterExecutingModeComplete();
    Fed2->finalize();

    auto core = Fed3->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    ASSERT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    // wait for the root broker to drop the lost federate; poll slowly, the broker skips timer
    // ticks while it is getting messages
    int cnt{0};
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    } while (federateState(*brk, "fed3") != "disconnected" && ++cnt < 8);
    ASSERT_EQ(federateState(*brk, "fed3"), "disconnected");
    // the subbroker should know as well
    EXPECT_EQ(federateState(*sub, "fed3"), "disconnected");

    // fed1 still waits on the lost federate until it comes back
    Fed1->requestTimeAsync(1.0);

    helics::FederateInfo fedInfo2(CORE_TYPE_TO_TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              sub->getIdentifier());
    helics::ValueFederate Fed3Redo("fed3", fedInfo2);
    auto& in1redo = Fed3Redo.registerSubscription("pub1");
    auto& pub3redo = Fed3Redo.registerGlobalPublication<double>("pub3");
    EXPECT_NO_THROW(Fed3Redo.enterExecutingMode());

    pub3redo.publish(-19.45);
    Fed3Redo.requestTimeAsync(2.0);
    EXPECT_EQ(Fed1->requestTimeComplete(), 1.0);
    EXPECT_DOUBLE_EQ(in3.getDouble(), -19.45);

    pub1.publish(19.45);
    Fed1->requestTimeAsync(2.0);
    EXPECT_EQ(Fed3Redo.requestTimeComplete(), 1.0);
    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);
    EXPECT_EQ(Fed3Redo.requestTime(2.0), 2.0);
    EXPECT_EQ(Fed1->requestTimeComplete(), 2.0);

    Fed1->finalize();
    Fed3Redo.finalize();
    core->disconnect();
    Fed3->finalize();
    sub->waitForDisconnect(std::chrono::milliseconds(500));
    brk->waitForDisconnect(std::chrono::milliseconds(500));
}

// a reentrant federate under a subbroker is lost and comes back on a core connected straight to
// the root, which records it against the subbroker
TEST(broker_timeout, reentrant_fed_lost_under_subbroker_rejoins_at_root)
{
    // the root finds the loss and leaves the decision to the subbroker, which has its default
    // timeout
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=1s --tick=200ms -f 3");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --broker=" + brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "lostroot_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);
    // keeps the subbroker up
    fedInfo.coreName = "lostroot_c2";
    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    fedInfo.coreName = "lostroot_c3";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed3 = std::make_shared<helics::ValueFederate>("fed3", fedInfo);

    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in3 = Fed1->registerSubscription("pub3");
    Fed3->registerGlobalPublication<double>("pub3");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingModeAsync();
    Fed3->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed2->enterExecutingModeComplete();

    auto core = Fed3->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    ASSERT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    int cnt{0};
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    } while (federateState(*brk, "fed3") != "disconnected" && ++cnt < 10);
    ASSERT_EQ(federateState(*brk, "fed3"), "disconnected");

    helics::FederateInfo fedInfo2(CORE_TYPE_TO_TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              brk->getIdentifier());
    helics::ValueFederate Fed3Redo("fed3", fedInfo2);
    auto& pub3redo = Fed3Redo.registerGlobalPublication<double>("pub3");
    EXPECT_NO_THROW(Fed3Redo.enterExecutingMode());

    pub3redo.publish(-19.45);
    Fed3Redo.requestTimeAsync(2.0);
    EXPECT_EQ(Fed1->requestTime(2.0), 1.0);
    EXPECT_DOUBLE_EQ(in3.getDouble(), -19.45);
    Fed3Redo.requestTimeComplete();

    Fed3Redo.finalize();
    Fed2->finalize();
    Fed1->finalize();
    core->disconnect();
    Fed3->finalize();
    sub->waitForDisconnect(std::chrono::milliseconds(500));
    brk->waitForDisconnect(std::chrono::milliseconds(500));
}

// a reentrant federate under a subbroker rejoins before the loss of its old core is noticed; the
// subbroker checks the old core when the federate registers again, the root does not notice
TEST(broker_timeout, reentrant_fed_rejoin_before_loss_under_subbroker)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=60s -f 2");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=2s --tick=200ms --broker=" +
                                                 brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "early_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);

    fedInfo.coreName = "early_c2";
    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    auto& pub1 = Fed1->registerGlobalPublication<double>("pub1");
    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in2 = Fed1->registerSubscription("pub2");
    Fed2->registerSubscription("pub1");
    Fed2->registerGlobalPublication<double>("pub2");

    Fed1->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    ASSERT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    Fed1->requestTimeAsync(1.0);

    helics::FederateInfo fedInfo2(CORE_TYPE_TO_TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              sub->getIdentifier());
    helics::ValueFederate Fed2Redo("fed2", fedInfo2);
    auto& in1redo = Fed2Redo.registerSubscription("pub1");
    auto& pub2redo = Fed2Redo.registerGlobalPublication<double>("pub2");
    EXPECT_NO_THROW(Fed2Redo.enterExecutingMode());

    pub2redo.publish(-19.45);
    Fed2Redo.requestTimeAsync(2.0);
    // fed1 only gets a grant once the new registration is used, so do not wait for the root's
    // 60 s timeout to find out
    for (int ii = 0; ii < 50 && !Fed1->isAsyncOperationCompleted(); ++ii) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!Fed1->isAsyncOperationCompleted()) {
        ADD_FAILURE() << "fed1 got no grant after the rejoin";
        brk->disconnect();
        return;
    }
    EXPECT_EQ(Fed1->requestTimeComplete(), 1.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -19.45);

    pub1.publish(19.45);
    Fed1->requestTimeAsync(2.0);
    EXPECT_EQ(Fed2Redo.requestTimeComplete(), 1.0);
    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);
    EXPECT_EQ(Fed2Redo.requestTime(2.0), 2.0);
    EXPECT_EQ(Fed1->requestTimeComplete(), 2.0);
    // both brokers still see the federate that came back
    EXPECT_NE(federateState(*brk, "fed2"), "disconnected");
    EXPECT_NE(federateState(*sub, "fed2"), "disconnected");

    Fed1->finalize();
    Fed2Redo.finalize();
    core->disconnect();
    Fed2->finalize();
    sub->disconnect();
    brk->disconnect();
}

// the id a broker reports for a federate
static int federateId(helics::Broker& broker, std::string_view name)
{
    auto state =
        helics::fileops::loadJsonStr(broker.query(broker.getIdentifier(), "current_state"));
    for (const auto& fed : state["federates"]) {
        if (fed["attributes"]["name"].get<std::string>() == name) {
            return fed["attributes"]["id"].get<int>();
        }
    }
    return -1;
}

// a disconnect sent up for the old registration of a reentrant federate under a subbroker
// reaches the root only after the federate has registered again
TEST(broker_timeout, reentrant_fed_late_disconnect_under_subbroker)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic -f 3");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --broker=" + brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "late_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);

    // a federate that keeps the subbroker up
    fedInfo.coreName = "late_c2";
    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    auto FedX = std::make_shared<helics::ValueFederate>("fedx", fedInfo);

    fedInfo.coreName = "late_c3";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    auto& pub1 = Fed1->registerGlobalPublication<double>("pub1");
    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in2 = Fed1->registerSubscription("pub2");
    Fed2->registerSubscription("pub1");
    Fed2->registerGlobalPublication<double>("pub2");

    Fed1->enterExecutingModeAsync();
    FedX->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    FedX->enterExecutingModeComplete();
    // the subbroker marks it disconnected and tells the root, numbered with the registration
    Fed2->finalize();
    EXPECT_EQ(Fed1->requestTime(1.0), 1.0);

    helics::FederateInfo fedInfo2(CORE_TYPE_TO_TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              sub->getIdentifier());
    helics::ValueFederate Fed2Redo("fed2", fedInfo2);
    auto& in1redo = Fed2Redo.registerSubscription("pub1");
    auto& pub2redo = Fed2Redo.registerGlobalPublication<double>("pub2");
    EXPECT_NO_THROW(Fed2Redo.enterExecutingMode());
    // make sure the new links have reached both cores
    Fed2Redo.query("root", "global_flush");

    // the root numbered the first registration 1; its disconnect arrives late
    helics::ActionMessage late(helics::CMD_DISCONNECT_FED);
    late.source_id = helics::GlobalFederateId(federateId(*brk, "fed2"));
    late.counter = 1;
    auto root = std::dynamic_pointer_cast<helics::CoreBroker>(brk);
    ASSERT_TRUE(root);
    root->addActionMessage(late);

    pub2redo.publish(-19.45);
    Fed2Redo.requestTimeAsync(2.0);
    EXPECT_EQ(Fed1->requestTime(2.0), 2.0);
    EXPECT_EQ(Fed2Redo.requestTimeComplete(), 2.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -19.45);

    pub1.publish(19.45);
    Fed2Redo.requestTimeAsync(3.0);
    EXPECT_EQ(Fed1->requestTime(3.0), 3.0);
    EXPECT_EQ(Fed2Redo.requestTimeComplete(), 3.0);
    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);
    // the root still sees the federate that came back
    EXPECT_NE(federateState(*brk, "fed2"), "disconnected");

    FedX->finalize();
    Fed1->finalize();
    Fed2Redo.finalize();
    sub->waitForDisconnect(std::chrono::milliseconds(500));
    brk->waitForDisconnect(std::chrono::milliseconds(500));
}

// a reentrant federate under a subbroker is refused on a new core while its old instance still
// runs, and comes back once the old instance has left
TEST(broker_timeout, reentrant_fed_old_instance_leaves_under_subbroker)
{
    auto brk = helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic -f 3");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --broker=" + brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "old_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);

    // the old core holds the reentrant federate and one that stays
    fedInfo.coreName = "old_c2";
    fedInfo.coreInitString = "-f 2 --broker=" + sub->getIdentifier();
    auto FedX = std::make_shared<helics::ValueFederate>("fedx", fedInfo);
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    auto& pub1 = Fed1->registerGlobalPublication<double>("pub1");
    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in2 = Fed1->registerSubscription("pub2");
    Fed2->registerSubscription("pub1");
    Fed2->registerGlobalPublication<double>("pub2");

    Fed1->enterExecutingModeAsync();
    FedX->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    FedX->enterExecutingModeComplete();

    helics::FederateInfo fedInfo2(CORE_TYPE_TO_TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              sub->getIdentifier());
    // the old instance is still running
    fedInfo2.coreName = "old_c3";
    EXPECT_THROW(helics::ValueFederate("fed2", fedInfo2), helics::RegistrationFailure);
    auto refusedCore = helics::CoreFactory::findCore("old_c3");
    if (refusedCore) {
        refusedCore->disconnect();
    }

    Fed2->finalize();
    EXPECT_EQ(Fed1->requestTime(1.0), 1.0);
    fedInfo2.coreName = "old_c4";
    helics::ValueFederate Fed2Redo("fed2", fedInfo2);
    auto& in1redo = Fed2Redo.registerSubscription("pub1");
    auto& pub2redo = Fed2Redo.registerGlobalPublication<double>("pub2");
    EXPECT_NO_THROW(Fed2Redo.enterExecutingMode());
    Fed2Redo.query("root", "global_flush");
    EXPECT_NE(federateState(*sub, "fed2"), "disconnected");
    EXPECT_NE(federateState(*brk, "fed2"), "disconnected");

    pub2redo.publish(-19.45);
    Fed2Redo.requestTimeAsync(2.0);
    EXPECT_EQ(Fed1->requestTime(2.0), 2.0);
    EXPECT_EQ(Fed2Redo.requestTimeComplete(), 2.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -19.45);

    pub1.publish(19.45);
    Fed2Redo.requestTimeAsync(3.0);
    EXPECT_EQ(Fed1->requestTime(3.0), 3.0);
    EXPECT_EQ(Fed2Redo.requestTimeComplete(), 3.0);
    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);

    FedX->finalize();
    Fed1->finalize();
    Fed2Redo.finalize();
    sub->disconnect();
    brk->disconnect();
}

// as reentrant_rejoin_disconnect_after_registration, with both cores under a subbroker that
// holds the new registration
TEST(broker_timeout, reentrant_fed_disconnect_after_registration_under_subbroker)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--dynamic --timeout=2s --tick=200ms");
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--dynamic --timeout=2s --tick=200ms --broker=" +
                                                 brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "subslow1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);

    // the old core holds the reentrant federate and one that stays
    fedInfo.coreName = "subslow2";
    fedInfo.coreInitString = "-f 2 --broker=" + sub->getIdentifier();
    auto FedX = std::make_shared<helics::ValueFederate>("fedx", fedInfo);
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    Fed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in2 = Fed1->registerSubscription("pub2");
    Fed2->registerGlobalPublication<double>("pub2");

    Fed1->enterExecutingModeAsync();
    FedX->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    FedX->enterExecutingModeComplete();

    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(Fed2->getCorePointer());
    ASSERT_TRUE(tcr);
    auto comms = tcr->getCommsObjectPointer();
    comms->pauseComms(true);
    Fed2->finalizeAsync();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    helics::FederateInfo fedInfo2(CORE_TYPE_TO_TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              sub->getIdentifier());
    fedInfo2.coreName = "subslow3";
    auto redo = std::async(std::launch::async, [&fedInfo2]() {
        return std::make_unique<helics::ValueFederate>("fed2", fedInfo2);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    comms->pauseComms(false);

    std::unique_ptr<helics::ValueFederate> Fed2Redo;
    EXPECT_NO_THROW(Fed2Redo = redo.get());
    Fed2->finalizeComplete();
    if (Fed2Redo) {
        auto& pub2redo = Fed2Redo->registerGlobalPublication<double>("pub2");
        EXPECT_NO_THROW(Fed2Redo->enterExecutingMode());
        if (Fed2Redo->getCurrentMode() == helics::Federate::Modes::EXECUTING) {
            pub2redo.publish(-4.5);
            Fed2Redo->requestTimeAsync(5.0);
            EXPECT_GT(Fed1->requestTime(5.0), helics::timeZero);
            EXPECT_DOUBLE_EQ(in2.getDouble(), -4.5);
            Fed2Redo->requestTimeComplete();
        }
        Fed2Redo->finalize();
    }
    FedX->finalize();
    Fed1->finalize();
    sub->disconnect();
    brk->disconnect();
}

// lose the core of a federate that is not reentrant while two others depend on it; with
// --disconnect_on_lost_connection the lost federate is disconnected and the others keep going
static void loseCoreWithFlag(const std::string& tag, bool flag)
{
    const std::string flagArg = flag ? " --disconnect_on_lost_connection" : "";
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=1s --tick=200ms" + flagArg);
    brk->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "cflag1" + tag;
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);
    fedInfo.coreName = "cflag3" + tag;
    auto Fed3 = std::make_shared<helics::ValueFederate>("test3", fedInfo);
    fedInfo.coreName = "cflaglost" + tag;
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    Fed2->registerGlobalPublication<double>("pflag2");
    Fed1->registerSubscription("pflag2");
    Fed3->registerSubscription("pflag2");

    Fed1->enterExecutingModeAsync();
    Fed3->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed3->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    if (flag) {
        // both wait on the lost federate until the broker finds it gone, then carry on
        for (int step = 1; step <= 5; ++step) {
            Fed3->requestTimeAsync(step);
            EXPECT_DOUBLE_EQ(static_cast<double>(Fed1->requestTime(step)), step);
            EXPECT_DOUBLE_EQ(static_cast<double>(Fed3->requestTimeComplete()), step);
        }
        EXPECT_TRUE(brk->isConnected());
        EXPECT_EQ(coreState(*brk, "cflaglost" + tag), "disconnected");
    } else {
        EXPECT_THROW(Fed1->requestTime(1.0), helics::HelicsException);
    }
    Fed1->finalize();
    Fed3->finalize();
    core->disconnect();
    brk->disconnect();
}

TEST(broker_timeout, lost_connection_disconnect)
{
    loseCoreWithFlag("on", true);
}

TEST(broker_timeout, lost_connection_error_without_flag)
{
    loseCoreWithFlag("off", false);
}

// the same under a subbroker; rootArgs and subArgs say which broker has the flag. Only the root
// pings the cores, so the root finds the loss and the subbroker decides on it
static void loseCoreUnderSubbrokerWithFlag(const std::string& tag,
                                           const std::string& rootArgs,
                                           const std::string& subArgs)
{
    auto brk =
        helics::BrokerFactory::create(CORE_TYPE_TO_TEST, "--timeout=1s --tick=200ms " + rootArgs);
    brk->connect();
    auto sub = helics::BrokerFactory::create(CORE_TYPE_TO_TEST,
                                             "--broker=" + brk->getIdentifier() + " " + subArgs);
    sub->connect();

    helics::FederateInfo fedInfo(CORE_TYPE_TO_TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "csflagroot" + tag;
    auto Fed1 = std::make_shared<helics::ValueFederate>("test1", fedInfo);

    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.coreName = "csflagother" + tag;
    auto Fed3 = std::make_shared<helics::ValueFederate>("test3", fedInfo);
    fedInfo.coreName = "csflaglost" + tag;
    auto Fed2 = std::make_shared<helics::ValueFederate>("test2", fedInfo);

    Fed2->registerGlobalPublication<double>("psflag2");
    Fed1->registerSubscription("psflag2");
    Fed3->registerSubscription("psflag2");

    Fed1->enterExecutingModeAsync();
    Fed3->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    Fed3->enterExecutingModeComplete();

    auto core = Fed2->getCorePointer();
    auto tcr = std::dynamic_pointer_cast<helics::testcore::TestCore>(core);
    EXPECT_TRUE(tcr);
    tcr->getCommsObjectPointer()->haltComms();  // this will terminate communications abruptly
    tcr.reset();

    for (int step = 1; step <= 5; ++step) {
        Fed3->requestTimeAsync(step);
        EXPECT_DOUBLE_EQ(static_cast<double>(Fed1->requestTime(step)), step);
        EXPECT_DOUBLE_EQ(static_cast<double>(Fed3->requestTimeComplete()), step);
    }
    EXPECT_TRUE(brk->isConnected());
    EXPECT_TRUE(sub->isConnected());
    EXPECT_EQ(coreState(*brk, "csflaglost" + tag), "disconnected");
    EXPECT_EQ(coreState(*sub, "csflaglost" + tag), "disconnected");
    Fed1->finalize();
    Fed3->finalize();
    core->disconnect();
    sub->disconnect();
    brk->disconnect();
}

TEST(broker_timeout, lost_connection_disconnect_subbroker)
{
    // flag on the root, which tells the subbroker to disconnect the core
    loseCoreUnderSubbrokerWithFlag("root", "--disconnect_on_lost_connection", "");
    // flag on the subbroker only
    loseCoreUnderSubbrokerWithFlag("sub", "", "--disconnect_on_lost_connection");
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
