/*
Copyright (c) 2017-2026,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Energy
Innovation LLC.  See the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/

#include "ValueFederateTestTemplates.hpp"
#include "helics/application_api/BrokerApp.hpp"
#include "helics/application_api/CoreApp.hpp"
#include "helics/application_api/Subscriptions.hpp"
#include "helics/application_api/ValueFederate.hpp"
#include "helics/core/Core.hpp"
#include "helics/core/helics_definitions.hpp"
#include "helics/helics_enums.h"

#include <future>
#include <gtest/gtest.h>
#include <thread>
#ifndef HELICS_SHARED_LIBRARY
#    include "testFixtures.hpp"
#else
#    include "testFixtures_shared.hpp"
#endif
#include <fstream>
#include <streambuf>
#include <string>

/** these test cases test out the value federates
 */
class dynFed: public ::testing::Test, public FederateTestFixture {};

TEST_F(dynFed, initPubSubs)
{
    SetupTest<helics::ValueFederate>("test", 1, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");

    auto& in1 = vFed1->registerSubscription("pub1");

    vFed1->enterInitializingMode();

    auto& pub2 = vFed1->registerGlobalPublication<double>("pub2");

    auto& in2 = vFed1->registerSubscription("pub2");

    vFed1->enterExecutingMode();

    pub1.publish(12.0);
    pub2.publish(13.3);
    vFed1->requestNextStep();
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), 13.3);

    vFed1->disconnect();
}

TEST_F(dynFed, execPubSubs)
{
    SetupTest<helics::ValueFederate>("test", 1, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");

    auto& in1 = vFed1->registerSubscription("pub1");

    vFed1->enterExecutingMode();

    auto& pub2 = vFed1->registerGlobalPublication<double>("pub2");

    auto& in2 = vFed1->registerSubscription("pub2");

    vFed1->requestNextStep();

    pub1.publish(12.0);
    pub2.publish(13.3);
    vFed1->requestNextStep();
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), 13.3);

    vFed1->disconnect();
}

TEST_F(dynFed, initPub_disable)
{
    extraCoreArgs = "--disable_dynamic_sources";
    SetupTest<helics::ValueFederate>("test", 1, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);

    vFed1->enterInitializingMode();

    EXPECT_THROW(vFed1->registerGlobalPublication<double>("pub2"), helics::RegistrationFailure);

    vFed1->disconnect();
}

TEST_F(dynFed, execPubSubs_disable)
{
    extraBrokerArgs = "--disable_dynamic_sources";
    SetupTest<helics::ValueFederate>("test", 1, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);

    vFed1->registerGlobalPublication<double>("pub1");

    vFed1->registerSubscription("pub1");

    vFed1->enterExecutingMode();

    vFed1->registerGlobalPublication<double>("pub2");

    vFed1->registerSubscription("pub2");
    vFed1->query("root", "global_flush");
    EXPECT_THROW(vFed1->requestNextStep(), helics::RegistrationFailure);
    vFed1->disconnect();
}

TEST_F(dynFed, execPubSubs_2fed)
{
    SetupTest<helics::ValueFederate>("test_2", 2, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");

    auto& in1 = vFed2->registerSubscription("pub1");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    auto& pub2 = vFed2->registerGlobalPublication<double>("pub2");

    auto& in2 = vFed1->registerSubscription("pub2");

    vFed2->query("root", "global_flush");
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    pub2.publish(13.3);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), 13.3);

    vFed1->disconnect();
}

TEST_F(dynFed, dynamicPubSubs_2fed)
{
    extraBrokerArgs = "--dynamic";
    SetupTest<helics::ValueFederate>("test_2", 2, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");

    auto& in1 = vFed2->registerSubscription("pub1");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    auto& pub2 = vFed2->registerGlobalPublication<double>("pub2");

    auto& in2 = vFed1->registerSubscription("pub2");

    vFed2->query("root", "global_flush");
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    pub2.publish(13.3);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), 13.3);
    EXPECT_TRUE(brokers[0]->isOpenToNewFederates());
    AddFederates<helics::ValueFederate>("test_2", 1, brokers[0], 1.0);
    auto vFed3 = GetFederateAs<helics::ValueFederate>(2);
    auto& pub3 = vFed3->registerGlobalPublication<double>("pub3");
    auto& in3 = vFed1->registerSubscription("pub3");
    auto& in1_3 = vFed3->registerSubscription("pub1");
    vFed3->enterExecutingMode();

    auto ctime = vFed3->getCurrentTime();
    // should be granted 1 valid time step behind where its dependencies are currently granted
    EXPECT_EQ(ctime, 1.0);

    pub1.publish(8.5);
    // requesting time 3
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestTimeAsync(helics::timeZero);
    ctime = vFed3->requestNextStep();

    // should be granted time 2 now
    EXPECT_EQ(ctime, 2.0);
    pub3.publish(7.5);
    ctime = vFed3->requestNextStep();
    EXPECT_EQ(ctime, 3.0);
    vFed1->requestTimeComplete();
    tres = vFed2->requestTimeComplete();

    EXPECT_DOUBLE_EQ(in1.getDouble(), 8.5);
    EXPECT_DOUBLE_EQ(in3.getDouble(), 7.5);

    pub1.publish(10.2);
    vFed2->disconnect();
    vFed1->disconnect();
    ctime = vFed3->requestNextStep();
    // due to connections this won't have connected until the next time request
    EXPECT_EQ(ctime, 4.0);
    EXPECT_EQ(in1_3.getDouble(), 10.2);
    vFed3->disconnect();
    std::this_thread::yield();
    int cnt{0};
    while (brokers[0]->isOpenToNewFederates()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        ++cnt;
        if (cnt > 10) {
            break;
        }
    }
    EXPECT_FALSE(brokers[0]->isOpenToNewFederates());
}

TEST_F(dynFed, dynamicPubSubs_2fed_pubStore)
{
    extraBrokerArgs = "--dynamic";
    SetupTest<helics::ValueFederate>("test_2", 2, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    pub1.setOption(HELICS_HANDLE_OPTION_BUFFER_DATA);

    auto& in1 = vFed2->registerSubscription("pub1");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    auto& pub2 = vFed2->registerGlobalPublication<double>("pub2");

    auto& in2 = vFed1->registerSubscription("pub2");

    vFed2->query("root", "global_flush");
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    pub2.publish(13.3);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), 13.3);
    EXPECT_TRUE(brokers[0]->isOpenToNewFederates());
    AddFederates<helics::ValueFederate>("test_2", 1, brokers[0], 1.0);
    auto vFed3 = GetFederateAs<helics::ValueFederate>(2);
    auto& pub3 = vFed3->registerGlobalPublication<double>("pub3");
    auto& in3 = vFed1->registerSubscription("pub3");
    auto& in1_3 = vFed3->registerSubscription("pub1");
    vFed3->enterExecutingMode();

    auto ctime = vFed3->getCurrentTime();
    // should be granted 1 valid time step behind where its dependencies are currently granted
    EXPECT_EQ(ctime, 1.0);

    pub1.publish(8.5);
    // requesting time 3
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestTimeAsync(helics::timeZero);
    ctime = vFed3->requestNextStep();

    // should be granted time 2 now
    EXPECT_EQ(ctime, 2.0);
    pub3.publish(7.5);
    ctime = vFed3->requestNextStep();
    EXPECT_EQ(ctime, 3.0);
    vFed1->requestTimeComplete();
    tres = vFed2->requestTimeComplete();

    EXPECT_DOUBLE_EQ(in1.getDouble(), 8.5);
    EXPECT_DOUBLE_EQ(in3.getDouble(), 7.5);
    EXPECT_DOUBLE_EQ(in1_3.getDouble(), 8.5);

    vFed2->disconnect();
    vFed1->disconnect();
    vFed3->disconnect();

    std::this_thread::yield();
    int cnt{0};
    while (brokers[0]->isOpenToNewFederates()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        ++cnt;
        if (cnt > 10) {
            break;
        }
    }
    EXPECT_FALSE(brokers[0]->isOpenToNewFederates());
}

TEST_F(dynFed, reentrant_fed_input)
{
    extraBrokerArgs = "--dynamic";
    extraFederateArgs = " --reentrant";
    SetupTest<helics::ValueFederate>("test_2", 2, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    pub1.setOption(HELICS_HANDLE_OPTION_BUFFER_DATA);

    auto& in1 = vFed2->registerSubscription("pub1");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);

    auto vFed2Name = vFed2->getName();
    vFed2->disconnect();
    tres = vFed1->requestTime(helics::timeZero);
    EXPECT_LE(tres, 3.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--reentrant --force_new_core --dynamic --broker=") +
                             brokers[0]->getIdentifier() + " --period=1.0s");

    helics::ValueFederate vFed2Redo(vFed2Name, fedInfo);
    EXPECT_EQ(vFed2Redo.getTimeProperty(HELICS_PROPERTY_TIME_PERIOD), 1.0);
    auto& in1redo = vFed2Redo.registerSubscription("pub1");
    vFed2Redo.enterExecutingMode();
    tres = vFed2Redo.getCurrentTime();
    EXPECT_LE(tres, 3.0);
    vFed2Redo.requestTimeAsync(helics::timeZero);

    pub1.publish(19.45);
    vFed1->requestTimeAsync(helics::timeZero);

    tres = vFed2Redo.requestTimeComplete();

    EXPECT_EQ(tres, 3.0);

    vFed2Redo.requestTimeAsync(helics::timeZero);
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 4.0);
    vFed1->disconnect();

    tres = vFed2Redo.requestTimeComplete();
    EXPECT_EQ(tres, 4.0);

    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);

    vFed2Redo.disconnect();
}

TEST_F(dynFed, reentrant_fed_publication)
{
    extraBrokerArgs = "--dynamic";
    extraFederateArgs = " --reentrant";
    SetupTest<helics::ValueFederate>("test_2", 2, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    vFed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in2 = vFed1->registerSubscription("pub2");
    pub1.setOption(HELICS_HANDLE_OPTION_BUFFER_DATA);

    auto& in1 = vFed2->registerSubscription("pub1");
    auto& pub2 = vFed2->registerGlobalPublication<double>("pub2");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    pub2.publish(-12.0);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -12.0);

    auto vFed2Name = vFed2->getName();
    vFed2->disconnect();
    tres = vFed1->requestTime(helics::timeZero);
    EXPECT_LE(tres, 3.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--reentrant --force_new_core --dynamic --broker=") +
                             brokers[0]->getIdentifier() + " --period=1.0s");

    helics::ValueFederate vFed2Redo(vFed2Name, fedInfo);
    EXPECT_EQ(vFed2Redo.getTimeProperty(HELICS_PROPERTY_TIME_PERIOD), 1.0);
    auto& in1redo = vFed2Redo.registerSubscription("pub1");

    auto& pub2redo = vFed2Redo.registerGlobalPublication<double>("pub2");
    vFed2Redo.enterExecutingMode();
    tres = vFed2Redo.getCurrentTime();
    EXPECT_LE(tres, 3.0);

    pub1.publish(19.45);
    pub2redo.publish(-19.45);
    vFed2Redo.requestTimeAsync(helics::timeZero);
    vFed1->requestTimeAsync(helics::timeZero);

    tres = vFed2Redo.requestTimeComplete();

    EXPECT_EQ(tres, 3.0);

    vFed2Redo.requestTimeAsync(helics::timeZero);
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 4.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -19.45);
    vFed1->disconnect();

    tres = vFed2Redo.requestTimeComplete();
    EXPECT_EQ(tres, 4.0);

    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);

    vFed2Redo.disconnect();
}

TEST_F(dynFed, dynamic_fed_new_subbroker)
{
    extraBrokerArgs = "--dynamic";
    SetupTest<helics::ValueFederate>("test_2", 1, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    vFed1->enterExecutingMode();
    EXPECT_EQ(vFed1->requestTime(1.0), 1.0);

    // a new subbroker joins the running federation, then a new federate through it
    auto sub = AddBroker("test", "--broker=" + brokers[0]->getIdentifier());
    ASSERT_TRUE(sub->isConnected());

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--force_new_core --dynamic --period=1.0s --broker=") +
                             sub->getIdentifier());
    helics::ValueFederate vFed2("late_fed", fedInfo);
    auto& in1 = vFed2.registerSubscription("pub1");

    vFed1->requestTimeAsync(2.0);
    vFed2.enterExecutingMode();
    EXPECT_LE(vFed2.getCurrentTime(), 1.0);
    // make sure the new links have reached both cores
    vFed2.query("root", "global_flush");
    vFed2.requestTimeAsync(2.0);
    EXPECT_EQ(vFed1->requestTimeComplete(), 2.0);
    EXPECT_EQ(vFed2.requestTimeComplete(), 2.0);
    // vFed1 may have been granted time 2 before the new subscriber reached it; it takes the
    // subscriber in during its next time request
    vFed1->requestTimeAsync(3.0);
    EXPECT_EQ(vFed2.requestTime(3.0), 3.0);
    EXPECT_EQ(vFed1->requestTimeComplete(), 3.0);

    pub1.publish(15.0);
    vFed1->requestTimeAsync(4.0);
    EXPECT_EQ(vFed2.requestTime(4.0), 4.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 15.0);
    EXPECT_EQ(vFed1->requestTimeComplete(), 4.0);

    vFed1->disconnect();
    vFed2.disconnect();
}

TEST_F(dynFed, reentrant_fed_publication_subbroker)
{
    extraBrokerArgs = "--dynamic";
    extraFederateArgs = " --reentrant";
    auto broker = AddBroker("test", 3);
    // the reentrant federate and a second federate that keeps the subbroker up
    AddFederates<helics::ValueFederate>("test_3", 2, broker, 1.0);
    AddFederates<helics::ValueFederate>("test", 1, broker, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(2);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed3 = GetFederateAs<helics::ValueFederate>(1);
    ASSERT_EQ(brokers.size(), 2U);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    vFed1->setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in2 = vFed1->registerSubscription("pub2");
    pub1.setOption(HELICS_HANDLE_OPTION_BUFFER_DATA);

    auto& in1 = vFed2->registerSubscription("pub1");
    auto& pub2 = vFed2->registerGlobalPublication<double>("pub2");

    vFed1->enterExecutingModeAsync();
    vFed3->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();
    vFed3->enterExecutingModeComplete();

    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    pub2.publish(-12.0);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -12.0);

    auto vFed2Name = vFed2->getName();
    vFed2->disconnect();
    tres = vFed1->requestTime(helics::timeZero);
    EXPECT_LE(tres, 3.0);

    // rejoin through the same subbroker
    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--reentrant --force_new_core --dynamic --broker=") +
                             brokers[1]->getIdentifier() + " --period=1.0s");

    helics::ValueFederate vFed2Redo(vFed2Name, fedInfo);
    auto& in1redo = vFed2Redo.registerSubscription("pub1");
    auto& pub2redo = vFed2Redo.registerGlobalPublication<double>("pub2");
    vFed2Redo.enterExecutingMode();
    tres = vFed2Redo.getCurrentTime();
    EXPECT_LE(tres, 3.0);

    pub1.publish(19.45);
    pub2redo.publish(-19.45);
    vFed2Redo.requestTimeAsync(helics::timeZero);
    vFed1->requestTimeAsync(helics::timeZero);

    tres = vFed2Redo.requestTimeComplete();
    EXPECT_EQ(tres, 3.0);

    vFed2Redo.requestTimeAsync(helics::timeZero);
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 4.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -19.45);
    vFed1->disconnect();

    tres = vFed2Redo.requestTimeComplete();
    EXPECT_EQ(tres, 4.0);

    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);

    vFed2Redo.disconnect();
    EXPECT_NE(vFed3->getCurrentMode(), helics::Federate::Modes::ERROR_STATE);
    vFed3->disconnect();
}

TEST_F(dynFed, reentrant_fed_publication_same_subbroker)
{
    extraBrokerArgs = "--dynamic";
    extraFederateArgs = " --reentrant";
    auto broker = AddBroker("test", 1);
    AddFederates<helics::ValueFederate>("test_3", 1, broker, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    ASSERT_EQ(brokers.size(), 2U);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    vFed1->enterExecutingMode();
    EXPECT_EQ(vFed1->requestTime(1.0), 1.0);

    // a federate under the same subbroker subscribes after the publication is known there
    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--force_new_core --dynamic --period=1.0s --broker=") +
                             brokers[1]->getIdentifier());
    helics::ValueFederate vFed2("same_sub_fed", fedInfo);
    vFed2.setFlagOption(HELICS_HANDLE_OPTION_RECONNECTABLE);
    auto& in1 = vFed2.registerSubscription("pub1");
    vFed1->requestTimeAsync(2.0);
    vFed2.enterExecutingMode();
    EXPECT_LE(vFed2.getCurrentTime(), 1.0);
    // make sure the new links have reached both cores
    vFed2.query("root", "global_flush");
    EXPECT_EQ(vFed2.requestTime(2.0), 2.0);
    EXPECT_EQ(vFed1->requestTimeComplete(), 2.0);

    pub1.publish(12.0);
    vFed1->requestTimeAsync(3.0);
    EXPECT_EQ(vFed2.requestTime(3.0), 3.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);
    EXPECT_EQ(vFed1->requestTimeComplete(), 3.0);

    auto vFed1Name = vFed1->getName();
    vFed1->disconnect();
    EXPECT_EQ(vFed2.requestTime(4.0), 4.0);

    // rejoin through the same subbroker
    helics::FederateInfo fedInfo2(helics::CoreType::TEST);
    fedInfo2.loadInfoFromArgs(std::string("--reentrant --force_new_core --dynamic --broker=") +
                              brokers[1]->getIdentifier() + " --period=1.0s");
    helics::ValueFederate vFed1Redo(vFed1Name, fedInfo2);
    auto& pub1redo = vFed1Redo.registerGlobalPublication<double>("pub1");
    vFed1Redo.enterExecutingMode();
    EXPECT_LE(vFed1Redo.getCurrentTime(), 4.0);
    vFed1Redo.query("root", "global_flush");
    vFed1Redo.requestTimeAsync(5.0);
    EXPECT_EQ(vFed2.requestTime(5.0), 5.0);
    EXPECT_EQ(vFed1Redo.requestTimeComplete(), 5.0);

    pub1redo.publish(19.45);
    vFed1Redo.requestTimeAsync(6.0);
    EXPECT_EQ(vFed2.requestTime(6.0), 6.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 19.45);

    vFed2.disconnect();
    vFed1Redo.requestTimeComplete();
    vFed1Redo.disconnect();
}

TEST_F(dynFed, reentrant_fed_input_single_core)
{
    extraBrokerArgs = "--dynamic";
    extraFederateArgs = " --reentrant";
    extraCoreArgs = " --dynamic";
    SetupTest<helics::ValueFederate>("test", 2, 1.0);
    auto vFed1 = GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = GetFederateAs<helics::ValueFederate>(1);

    auto& pub1 = vFed1->registerGlobalPublication<double>("pub1");
    pub1.setOption(HELICS_HANDLE_OPTION_BUFFER_DATA);

    auto& in1 = vFed2->registerSubscription("pub1");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);

    pub1.publish(12.0);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 2.0);
    EXPECT_DOUBLE_EQ(in1.getDouble(), 12.0);

    auto vFed2Name = vFed2->getName();
    auto core = vFed2->getCorePointer();

    vFed2->disconnect();
    tres = vFed1->requestTime(helics::timeZero);
    EXPECT_LE(tres, 3.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--reentrant --dynamic --period=1.0s"));
    EXPECT_TRUE(core->isOpenToNewFederates());
    helics::ValueFederate vFed2Redo(vFed2Name, core, fedInfo);
    EXPECT_EQ(vFed2Redo.getTimeProperty(HELICS_PROPERTY_TIME_PERIOD), 1.0);
    auto& in1redo = vFed2Redo.registerSubscription("pub1");
    vFed2Redo.enterExecutingMode();
    tres = vFed2Redo.getCurrentTime();
    EXPECT_LE(tres, 3.0);
    vFed2Redo.requestTimeAsync(helics::timeZero);

    pub1.publish(19.45);
    vFed1->requestTimeAsync(helics::timeZero);

    tres = vFed2Redo.requestTimeComplete();

    EXPECT_EQ(tres, 3.0);

    vFed2Redo.requestTimeAsync(helics::timeZero);
    tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 4.0);
    vFed1->disconnect();

    tres = vFed2Redo.requestTimeComplete();
    EXPECT_EQ(tres, 4.0);

    EXPECT_DOUBLE_EQ(in1redo.getDouble(), 19.45);

    vFed2Redo.disconnect();
}

/** the subscription to pub2 is marked reconnectable only in the interface configuration, so it
must still get data from a publisher that leaves and rejoins*/
static void reentrantConfigReconnectable(FederateTestFixture& fixture, const std::string& config)
{
    fixture.extraBrokerArgs = "--dynamic";
    fixture.extraFederateArgs = " --reentrant";
    fixture.SetupTest<helics::ValueFederate>("test_2", 2, 1.0);
    auto vFed1 = fixture.GetFederateAs<helics::ValueFederate>(0);
    auto vFed2 = fixture.GetFederateAs<helics::ValueFederate>(1);

    vFed1->registerInterfaces(config);
    auto& in2 = vFed1->getInputByTarget("pub2");
    ASSERT_TRUE(in2.isValid());

    auto& pub2 = vFed2->registerGlobalPublication<double>("pub2");

    vFed1->enterExecutingModeAsync();
    vFed2->enterExecutingMode();
    vFed1->enterExecutingModeComplete();

    pub2.publish(-12.0);
    vFed1->requestTimeAsync(helics::timeZero);
    vFed2->requestNextStep();
    auto tres = vFed1->requestTimeComplete();
    EXPECT_EQ(tres, 1.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -12.0);

    auto vFed2Name = vFed2->getName();
    vFed2->disconnect();
    tres = vFed1->requestTime(helics::timeZero);
    EXPECT_EQ(tres, 2.0);

    helics::FederateInfo fedInfo(helics::CoreType::TEST);
    fedInfo.loadInfoFromArgs(std::string("--reentrant --force_new_core --dynamic --broker=") +
                             fixture.brokers[0]->getIdentifier() + " --period=1.0s");

    helics::ValueFederate vFed2Redo(vFed2Name, fedInfo);
    auto& pub2redo = vFed2Redo.registerGlobalPublication<double>("pub2");
    vFed2Redo.enterExecutingMode();

    pub2redo.publish(-19.45);
    vFed2Redo.requestTimeAsync(5.0);
    tres = vFed1->requestTime(3.0);
    EXPECT_EQ(tres, 3.0);
    EXPECT_DOUBLE_EQ(in2.getDouble(), -19.45);
    vFed1->disconnect();

    vFed2Redo.requestTimeComplete();
    vFed2Redo.disconnect();
}

TEST_F(dynFed, reentrant_fed_json_reconnectable_subscription)
{
    reentrantConfigReconnectable(
        *this, R"({"subscriptions":[{"key":"pub2","type":"double","reconnectable":true}]})");
}

TEST_F(dynFed, reentrant_fed_toml_reconnectable_subscription)
{
    reentrantConfigReconnectable(*this,
                                 "[[subscriptions]]\nkey=\"pub2\"\ntype=\"double\"\n"
                                 "reconnectable=true\n");
}
