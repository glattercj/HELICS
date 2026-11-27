/*
Copyright (c) 2017-2026,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Energy
Innovation LLC.  See the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/

#include "helics/application_api/Publications.hpp"
#include "helics/application_api/Subscriptions.hpp"

#include "gtest/gtest.h"
#include <complex>
#include <future>
#include <iostream>
#include <memory>
#include <string>

/** these test cases test out the value converters
 */
#include "../application_api/testFixtures.hpp"
#include "helics/common/JsonProcessingFunctions.hpp"
#include "helics/core/ActionMessage.hpp"
#include "helics/core/CoreBroker.hpp"
#include "helics/helics-config.h"
#include "helics/helics.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

struct dynFed: public FederateTestFixture, public ::testing::Test {};

/** just a check that in the simple case we do actually get the time back we requested*/
TEST_F(dynFed, simple_observer)
{
    SetupTest<helics::ValueFederate>("test_2", 2);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    vFed1->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    vFed2->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    auto& pub = vFed1->registerGlobalPublication<double>("pub1");
    vFed2->registerSubscription("pub1");
    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();
    pub.publish(0.27);
    auto res = vFed1->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 0.5);  // the result should show up at the next available time point
    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.observer = true;
    // now we try to join the federation with an observer
    auto bname = brokers[0]->getIdentifier();
    auto cdyn = helics::CoreFactory::create(helics::CoreType::TEST,
                                            "coredyn",
                                            std::string("--observer --broker=") + bname);

    EXPECT_TRUE(cdyn->connect());
    fedInfo.coreName = "coredyn";

    std::shared_ptr<helics::ValueFederate> fobs;
    EXPECT_NO_THROW(fobs = std::make_shared<helics::ValueFederate>("fedObs", fedInfo));

    if (fobs) {
        EXPECT_NO_THROW(fobs->enterInitializingMode());

        EXPECT_NO_THROW(fobs->enterExecutingMode());

        EXPECT_EQ(fobs->getCurrentTime(), helics::timeZero);
        fobs->disconnect();
    }

    cdyn->disconnect();

    vFed1->finalize();
    vFed2->finalize();
}

/** just a check that in the simple case we do actually get the time back we requested*/
TEST_F(dynFed, observer_subscriber)
{
    SetupTest<helics::ValueFederate>("test_2", 2);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    vFed1->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    vFed2->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    auto& pub = vFed1->registerGlobalPublication<double>("pub1");
    vFed2->registerSubscription("pub1");
    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();
    pub.publish(0.27);
    auto res = vFed1->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 0.5);  // the result should show up at the next available time point
    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.observer = true;

    auto bname = brokers[0]->getIdentifier();
    auto cdyn = helics::CoreFactory::create(helics::CoreType::TEST,
                                            "coredyn",
                                            std::string("--observer --broker=") + bname);

    EXPECT_TRUE(cdyn->connect());
    fedInfo.coreName = "coredyn";

    std::shared_ptr<helics::ValueFederate> fobs;
    EXPECT_NO_THROW(fobs = std::make_shared<helics::ValueFederate>("fedObs", fedInfo));
    // now we try to join the federation with an observer
    if (fobs) {
        fobs->registerSubscription("pub1");
        fobs->query("root", "global_flush");

        EXPECT_NO_THROW(fobs->enterInitializingMode());

        EXPECT_NO_THROW(fobs->enterExecutingMode());
        // should be a 1 time delta(1 ns) before 2.0
        EXPECT_NEAR(static_cast<double>(fobs->getCurrentTime()),
                    static_cast<double>(helics::Time(2.0)),
                    0.000000002);
        fobs->disconnect();
    }

    cdyn->disconnect();

    vFed1->finalize();
    vFed2->finalize();
}

/** now make sure we can get the values properly*/
TEST_F(dynFed, observer_subscriber_value)
{
    SetupTest<helics::ValueFederate>("test_2", 2);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    vFed1->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    vFed2->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    auto& pub = vFed1->registerGlobalPublication<double>("pub1");
    auto& sub = vFed2->registerSubscription("pub1");
    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();
    pub.publish(0.27);
    auto res = vFed1->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 0.5);  // the result should show up at the next available time point
    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.observer = true;

    auto bname = brokers[0]->getIdentifier();
    auto cdyn = helics::CoreFactory::create(helics::CoreType::TEST,
                                            "coredyn",
                                            std::string("--observer --broker=") + bname);

    EXPECT_TRUE(cdyn->connect());
    fedInfo.coreName = "coredyn";
    // now we try to join the federation with an observer
    std::shared_ptr<helics::ValueFederate> fobs;
    EXPECT_NO_THROW(fobs = std::make_shared<helics::ValueFederate>("fedObs", fedInfo));

    if (!fobs) {
        cdyn->disconnect();

        vFed1->finalize();
        vFed2->finalize();
        ASSERT_TRUE(fobs);
        return;
    }
    auto& obsSubs = fobs->registerSubscription("pub1");
    fobs->query("root", "global_flush");

    EXPECT_NO_THROW(fobs->enterInitializingMode());

    EXPECT_NO_THROW(fobs->enterExecutingMode());
    res = vFed1->requestTime(3.0);
    EXPECT_EQ(res, 3.0);
    pub.publish(0.8987);
    res = vFed1->requestTime(4.0);
    EXPECT_EQ(res, 4.0);

    res = vFed2->requestTime(4.0);
    EXPECT_EQ(res, 3.0);

    EXPECT_DOUBLE_EQ(sub.getValue<double>(), 0.8987);

    res = fobs->requestTime(4.0);
    EXPECT_EQ(res, 3.0);
    EXPECT_DOUBLE_EQ(obsSubs.getValue<double>(), 0.8987);

    fobs->finalize();
    cdyn->disconnect();

    vFed1->finalize();
    vFed2->finalize();
}

/** now make sure we can get the values properly*/
TEST_F(dynFed, observer_subscriber_value_with_buffer)
{
    SetupTest<helics::ValueFederate>("test_2", 2);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    vFed1->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    vFed2->setProperty(HELICS_PROPERTY_TIME_PERIOD, 0.5);
    auto& pub = vFed1->registerGlobalPublication<double>("pub1");
    pub.setOption(HELICS_HANDLE_OPTION_BUFFER_DATA);
    auto& sub = vFed2->registerSubscription("pub1");
    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();
    pub.publish(0.27);
    auto res = vFed1->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 0.5);  // the result should show up at the next available time point
    res = vFed2->requestTime(2.0);
    EXPECT_EQ(res, 2.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.observer = true;

    auto bname = brokers[0]->getIdentifier();
    auto cdyn = helics::CoreFactory::create(helics::CoreType::TEST,
                                            "coredyn",
                                            std::string("--observer --broker=") + bname);

    EXPECT_TRUE(cdyn->connect());
    fedInfo.coreName = "coredyn";
    // now we try to join the federation with an observer
    std::shared_ptr<helics::ValueFederate> fobs;
    EXPECT_NO_THROW(fobs = std::make_shared<helics::ValueFederate>("fedObs", fedInfo));

    if (!fobs) {
        cdyn->disconnect();

        vFed1->finalize();
        vFed2->finalize();
        ASSERT_TRUE(fobs);
        return;
    }
    auto& obsSubs = fobs->registerSubscription("pub1");
    fobs->query("root", "global_flush");

    EXPECT_NO_THROW(fobs->enterInitializingMode());

    EXPECT_NO_THROW(fobs->enterExecutingMode());
    EXPECT_DOUBLE_EQ(obsSubs.getValue<double>(), 0.27);
    res = vFed1->requestTime(3.0);
    EXPECT_EQ(res, 3.0);
    pub.publish(0.8987);
    res = vFed1->requestTime(4.0);
    EXPECT_EQ(res, 4.0);

    res = vFed2->requestTime(4.0);
    EXPECT_EQ(res, 3.0);

    EXPECT_DOUBLE_EQ(sub.getValue<double>(), 0.8987);

    res = fobs->requestTime(4.0);
    EXPECT_EQ(res, 4.0);
    EXPECT_DOUBLE_EQ(obsSubs.getValue<double>(), 0.8987);

    fobs->finalize();
    cdyn->disconnect();

    vFed1->finalize();
    vFed2->finalize();
}

// the state and id a broker reports for a federate
static std::pair<std::string, int> brokerFedInfo(helics::Broker& broker, std::string_view name)
{
    auto state =
        helics::fileops::loadJsonStr(broker.query(broker.getIdentifier(), "current_state"));
    for (const auto& fed : state["federates"]) {
        if (fed["attributes"]["name"].get<std::string>() == name) {
            return {fed["state"].get<std::string>(), fed["attributes"]["id"].get<int>()};
        }
    }
    return {std::string{}, -1};
}

// the state and id a broker reports for a core
static std::pair<std::string, int> brokerCoreInfo(helics::Broker& broker, std::string_view name)
{
    auto state =
        helics::fileops::loadJsonStr(broker.query(broker.getIdentifier(), "current_state"));
    for (const auto& core : state["cores"]) {
        if (core["attributes"]["name"].get<std::string>() == name) {
            return {core["state"].get<std::string>(), core["attributes"]["id"].get<int>()};
        }
    }
    return {std::string{}, -1};
}

// counts the trace lines of a broker that contain one string and end with another
class TraceCounter {
  public:
    TraceCounter(helics::Broker& broker, std::string part, std::string suffix):
        mBroker(broker), mPart(std::move(part)), mSuffix(std::move(suffix))
    {
        mBroker.setLoggingCallback(
            [this](int /*level*/, std::string_view /*source*/, std::string_view message) {
                if (message.find(mPart) != std::string_view::npos && message.ends_with(mSuffix)) {
                    ++mCount;
                }
            });
        mBroker.setLoggingLevel(HELICS_LOG_LEVEL_TRACE);
    }
    ~TraceCounter()
    {
        mBroker.setLoggingLevel(HELICS_LOG_LEVEL_WARNING);
        mBroker.setLoggingCallback({});
    }
    TraceCounter(const TraceCounter&) = delete;
    TraceCounter& operator=(const TraceCounter&) = delete;
    int count() const { return mCount.load(); }

  private:
    helics::Broker& mBroker;
    std::string mPart;
    std::string mSuffix;
    std::atomic<int> mCount{0};
};

// a reentrant federate leaves from under a subbroker and takes its core with it; a value, a query
// reply or a command still addressed to it, or a ping to its core, must not travel between the
// subbroker and the root
TEST(dynamic_federation, gone_federate_under_subbroker_no_route_loop)
{
    auto brk = helics::BrokerFactory::create(helics::CoreType::TEST, "--dynamic -f 3");
    brk->connect();
    auto sub = helics::BrokerFactory::create(helics::CoreType::TEST,
                                             "--dynamic --broker=" + brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "loop_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);

    // a federate on its own core that keeps the subbroker up
    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.coreName = "loop_c2";
    auto FedX = std::make_shared<helics::ValueFederate>("fedx", fedInfo);

    fedInfo.coreName = "loop_c3";
    fedInfo.setFlagOption(HELICS_FLAG_REENTRANT);
    auto Fed2 = std::make_shared<helics::ValueFederate>("fed2", fedInfo);

    auto& pub1 = Fed1->registerGlobalPublication<double>("pub1");
    Fed2->registerSubscription("pub1");
    Fed2->registerGlobalPublication<double>("pub2");

    Fed1->enterExecutingModeAsync();
    FedX->enterExecutingModeAsync();
    Fed2->enterExecutingMode();
    Fed1->enterExecutingModeComplete();
    FedX->enterExecutingModeComplete();
    Fed2->finalize();

    int cnt{0};
    while (brokerFedInfo(*sub, "fed2").first != "disconnected" && ++cnt < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_EQ(brokerFedInfo(*sub, "fed2").first, "disconnected");
    const int fed2Id = brokerFedInfo(*brk, "fed2").second;
    ASSERT_GT(fed2Id, 0);
    const int fed1Id = brokerFedInfo(*brk, "fed1").second;
    auto root = std::dynamic_pointer_cast<helics::CoreBroker>(brk);
    ASSERT_TRUE(root);

    {
        const std::string toFed2 = " to " + std::to_string(fed2Id);
        TraceCounter atRoot(*brk, "|| cmd:", toFed2);
        TraceCounter atSub(*sub, "|| cmd:", toFed2);
        // a value from fed1 that was still on its way to fed2
        helics::ActionMessage late(helics::CMD_PUB);
        late.source_id = helics::GlobalFederateId(fed1Id);
        late.source_handle = pub1.getHandle();
        late.dest_id = helics::GlobalFederateId(fed2Id);
        late.dest_handle = helics::InterfaceHandle(0);
        late.payload = std::string_view("late");
        root->addActionMessage(late);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        // each broker sees it at most once
        EXPECT_LE(atRoot.count(), 1);
        EXPECT_LE(atSub.count(), 1);
    }

    // priority commands take another path through the broker
    for (auto action : {helics::CMD_QUERY_REPLY, helics::CMD_SEND_COMMAND}) {
        helics::ActionMessage late(action);
        late.source_id = helics::GlobalFederateId(fed1Id);
        late.dest_id = helics::GlobalFederateId(fed2Id);
        late.payload = std::string_view("late");
        const std::string line = "|| priority_cmd:" + helics::prettyPrintString(late);
        const std::string fromFed1 = " from " + std::to_string(fed1Id);
        TraceCounter atRoot(*brk, line, fromFed1);
        TraceCounter atSub(*sub, line, fromFed1);
        root->addActionMessage(late);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        EXPECT_LE(atRoot.count(), 1) << line;
        EXPECT_LE(atSub.count(), 1) << line;
    }

    // a ping from the root to the core that left with fed2
    cnt = 0;
    while (brokerCoreInfo(*sub, "loop_c3").first != "disconnected" && ++cnt < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_EQ(brokerCoreInfo(*sub, "loop_c3").first, "disconnected");
    const int coreId = brokerCoreInfo(*brk, "loop_c3").second;
    ASSERT_GT(coreId, 0);
    {
        const std::string toCore = " to " + std::to_string(coreId);
        TraceCounter atRoot(*brk, "|| cmd:ping", toCore);
        TraceCounter atSub(*sub, "|| cmd:ping", toCore);
        helics::ActionMessage ping(helics::CMD_PING);
        ping.source_id = helics::gRootBrokerID;
        ping.dest_id = helics::GlobalFederateId(coreId);
        root->addActionMessage(ping);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        EXPECT_LE(atRoot.count(), 1);
        EXPECT_LE(atSub.count(), 1);
    }

    pub1.publish(19.45);
    EXPECT_EQ(Fed1->requestTime(1.0), 1.0);
    FedX->finalize();
    Fed1->finalize();
    sub->waitForDisconnect(std::chrono::milliseconds(500));
    brk->waitForDisconnect(std::chrono::milliseconds(500));
}

// a reentrant federate that left from under a subbroker, and that the root has marked
// disconnected, still gets data both ways after it rejoins through the root
TEST(dynamic_federation, gone_federate_under_subbroker_rejoins)
{
    auto brk = helics::BrokerFactory::create(helics::CoreType::TEST, "--dynamic -f 2");
    brk->connect();
    auto sub = helics::BrokerFactory::create(helics::CoreType::TEST,
                                             "--dynamic --broker=" + brk->getIdentifier());
    sub->connect();

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.setProperty(HELICS_PROPERTY_TIME_PERIOD, 1.0);
    fedInfo.coreInitString = "--broker=" + brk->getIdentifier();
    fedInfo.coreName = "back_c1";
    auto Fed1 = std::make_shared<helics::ValueFederate>("fed1", fedInfo);

    // fed2 shares its core with fedx, so it leaves as a federate, not with its core
    fedInfo.coreInitString = "--broker=" + sub->getIdentifier();
    fedInfo.coreName = "back_c2";
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
    Fed2->finalize();

    int cnt{0};
    while (brokerFedInfo(*brk, "fed2").first != "disconnected" && ++cnt < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_EQ(brokerFedInfo(*brk, "fed2").first, "disconnected");
    pub1.publish(1.5);
    EXPECT_EQ(Fed1->requestTime(1.0), 1.0);

    helics::FederateInfo fedInfo2(helics::CoreType::TEST);
    fedInfo2.loadInfoFromArgs("--reentrant --force_new_core --dynamic --period=1.0s --broker=" +
                              brk->getIdentifier());
    helics::ValueFederate Fed2Redo("fed2", fedInfo2);
    auto& in1redo = Fed2Redo.registerSubscription("pub1");
    auto& pub2redo = Fed2Redo.registerGlobalPublication<double>("pub2");
    EXPECT_NO_THROW(Fed2Redo.enterExecutingMode());
    Fed2Redo.query("root", "global_flush");

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

    Fed2Redo.finalize();
    FedX->finalize();
    Fed1->finalize();
    sub->waitForDisconnect(std::chrono::milliseconds(500));
    brk->waitForDisconnect(std::chrono::milliseconds(500));
}
