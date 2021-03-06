/*
 ** Author(s):
 **  - Ly William Chhim <lwchhim@aldebaran-robotics.com>
 **
 ** Copyright (C) 2013 Aldebaran Robotics
 */

#include <gtest/gtest.h>

#include <qi/applicationsession.hpp>
#include <qi/future.hpp>

static bool _stopped = false;
static qi::Session _sd;
static qi::ApplicationSession* _app;
static char **_argv = nullptr;
static int _argc = 5;
static std::string _url;
static qi::Promise<void> _sync;

void onStop()
{
  _stopped = true;
  _sync.setValue(0);
}

TEST(QiApplicationSession, defaultConnect)
{
  ASSERT_FALSE(_app->session()->isConnected());
  _app->startSession();
  ASSERT_TRUE(_app->session()->isConnected());

  ASSERT_EQ(_url, _app->session()->url());

  ASSERT_FALSE(_stopped);
  _sd.close();
  _sync.future().wait();
  ASSERT_TRUE(_stopped);

  EXPECT_THROW(_app->session()->connect("ftp://invalidurl:42"),
               qi::FutureUserException);
  ASSERT_FALSE(_app->session()->isConnected());
}

TEST(QiApplicationSession, checkArgs)
{
  EXPECT_EQ(1, _app->argc());
  EXPECT_EQ(std::string("foo"), _app->argv()[0]);

  EXPECT_EQ(1, _argc);
  EXPECT_EQ(std::string("foo"), _argv[0]);
}

TEST(QiApplicationSession, defineConfig)
{
  qi::ApplicationSession::Config config;
  config.setDefaultStandAlone(true);
  config.setDefaultListenUrl("tcp://localhost:0");
  config.setName("AppSessionTest");
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  _sd.listenStandalone("tcp://127.0.0.1:0");
  _url = _sd.endpoints()[0].str();

  _argv = new char*[6];
  strcpy((_argv[0] = new char[4]), "foo");
  strcpy((_argv[1] = new char[10]), "--qi-url");
  strcpy((_argv[2] = new char[100]), _url.c_str());
  strcpy((_argv[3] = new char[20]), "--qi-listen-url");
  strcpy((_argv[4] = new char[100]), "tcp://localhost:0");
  _argv[5] = 0;

  qi::ApplicationSession app(_argc, _argv, qi::ApplicationSession::Option_None, "This url will be ignored");
  app.atStop(&onStop);
  _app = &app;
  return RUN_ALL_TESTS();
}
