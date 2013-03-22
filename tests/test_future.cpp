/*
** Author(s):
**  - Cedric GESTES <gestes@aldebaran-robotics.com>
**
** Copyright (C) 2010, 2012 Aldebaran Robotics
*/

#include <map>
#include <string>
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <qi/os.hpp>
#include <qi/log.hpp>
#include <qi/atomic.hpp>
#include <qi/application.hpp>
#include <qi/future.hpp>
#include <qi/eventloop.hpp>

class TestFuture : public ::testing::Test
{
public:
  static int         gGlobalI;
  static std::string gGlobalS;
  static std::string gGlobalE;
  static qi::Atomic<int>   gSuccess;

protected:
  static void SetUpTestCase()
  {
    gGlobalI = 0;
    gGlobalS = "";
    gGlobalE = "";
    gSuccess = 0;
  }
};

int         TestFuture::gGlobalI;
std::string TestFuture::gGlobalS;
std::string TestFuture::gGlobalE;
qi::Atomic<int> TestFuture::gSuccess;

class TestFutureI {
public:
  TestFutureI(int &gGlobalI, std::string &gGlobalE)
    : gGlobalI(gGlobalI),
      gGlobalE(gGlobalE)
  {}

  void onFutureFinished(qi::Future<int> future) {
    if (future.hasError())
      gGlobalE = future.error();
    else
      gGlobalI = future.value();
  }

  int         &gGlobalI;
  std::string &gGlobalE;
};

class TestFutureS {
public:
  TestFutureS(std::string &gGlobalS, std::string &gGlobalE)
    : gGlobalS(gGlobalS),
      gGlobalE(gGlobalE)
  {}

  void onFutureFinished(qi::Future<std::string> future) {
    if (future.hasError())
      gGlobalE = future.error();
    else
      gGlobalS = future.value();
  }

  std::string &gGlobalS;
  std::string &gGlobalE;
};

TEST_F(TestFuture, SimpleType) {
  TestFutureI tf(gGlobalI, gGlobalE);

  qi::Promise<int> pro;

  qi::Future<int>  fut = pro.future();

  fut.connect(boost::bind(&TestFutureI::onFutureFinished, tf, _1));

  EXPECT_EQ(0, gGlobalI);
  EXPECT_FALSE(fut.isFinished());
  EXPECT_TRUE(fut.isRunning());
  EXPECT_FALSE(fut.isCanceled());
  pro.setValue(42);
  fut.wait(1000);
  EXPECT_TRUE(fut.isFinished());
  EXPECT_EQ(42, fut.value());
  EXPECT_EQ(42, gGlobalI);
}

TEST_F(TestFuture, ComplexType) {
  TestFutureS tf(gGlobalS, gGlobalE);

  qi::Promise<std::string> pro;

  qi::Future<std::string>  fut = pro.future();

  fut.connect(boost::bind(&TestFutureS::onFutureFinished, tf, _1));

  EXPECT_STREQ("", gGlobalS.c_str());
  EXPECT_FALSE(fut.isFinished());
  pro.setValue("42");
  EXPECT_TRUE(fut.isFinished());
  EXPECT_STREQ("42", fut.value().c_str());
  EXPECT_STREQ("42", gGlobalS.c_str());
}

void producer(qi::Promise<int> pro) {
  qi::os::msleep(100);
  pro.setValue(42);
}

void consumer(qi::Atomic<int> &gSuccess, qi::Future<int> fut) {
  //wont block thread on error
  ASSERT_TRUE(fut.wait(1000) == qi::FutureState_FinishedWithValue);
  EXPECT_EQ(42, fut.value());
  ++gSuccess;
}

TEST_F(TestFuture, Threaded) {
  qi::Promise<int> pro;
  EXPECT_EQ(0, *gSuccess);
  boost::thread_group tg;

  tg.create_thread(boost::bind(&consumer, boost::ref(gSuccess), pro.future()));
  tg.create_thread(boost::bind(&consumer, boost::ref(gSuccess), pro.future()));
  tg.create_thread(boost::bind(&consumer, boost::ref(gSuccess), pro.future()));
  tg.create_thread(boost::bind(&producer, pro));
  tg.join_all();
  EXPECT_EQ(3, *gSuccess);
}


TEST_F(TestFuture, TestTimeout) {
  qi::Promise<int> pro;
  qi::Future<int>  fut = pro.future();

  EXPECT_TRUE(fut.wait(100) == qi::FutureState_Running);
  EXPECT_FALSE(fut.isFinished());
}

TEST_F(TestFuture, TestError) {
  TestFutureI tf(gGlobalI, gGlobalE);

  qi::Promise<int> pro;

  qi::Future<int>  fut = pro.future();
  fut.connect(boost::bind(&TestFutureI::onFutureFinished, tf, _1));

  EXPECT_STREQ("", gGlobalE.c_str());
  EXPECT_FALSE(fut.isFinished());
  pro.setError("chiche");
  fut.wait();
  EXPECT_STREQ("chiche", gGlobalE.c_str());
  EXPECT_TRUE(fut.isFinished());
  EXPECT_TRUE(fut.hasError());
  EXPECT_THROW(fut.value(), qi::FutureUserException);
  std::string err;
  try {
    fut.value();
  } catch (qi::FutureUserException &fue) {
    err = fue.what();
  }
  EXPECT_EQ(err, fut.error());
}

TEST_F(TestFuture, TestStateNone)
{
  qi::Future<int> f;
  ASSERT_FALSE(f.isRunning());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_FALSE(f.isFinished());
  ASSERT_TRUE(f.wait(qi::FutureTimeout_None) == qi::FutureState_None);

  qi::Promise<int> p;
  f = p.future();
  ASSERT_TRUE(f.isRunning());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_FALSE(f.isFinished());
  ASSERT_TRUE(f.wait(qi::FutureTimeout_None) == qi::FutureState_Running);
}

void unlock(qi::Promise<int> prom, bool* tag)
{
  *tag = true;
  prom.setValue(1);
}

TEST(TestFutureSync, Basic)
{
  qi::EventLoop* eventLoop = qi::getDefaultNetworkEventLoop();
  ASSERT_FALSE(eventLoop->isInEventLoopThread());

  {
    qi::FutureSync<int> fs;
    ASSERT_TRUE(!fs.isFinished());
  } // unbound futuresync should not block

  bool tag = false;
  {
    qi::FutureSync<int> fs;
    qi::Promise<int> p;
    fs = p.future();
    eventLoop->async(boost::bind(unlock, p, &tag), 50000);
  }
  ASSERT_TRUE(tag); // fs should block at end of scope, so we reach here after unlock

  tag = false;
  { // This scope is a synchro point to avoid tests running in //
    qi::Promise<int> p;
    qi::FutureSync<int> syncPoint = p.future();
    {
      qi::FutureSync<int> fs;
      fs = p.future();
      fs.async();
      eventLoop->async(boost::bind(unlock, p, &tag), 50000);
    }
    ASSERT_FALSE(tag); // fs is async: we exit immediately
  }
  ASSERT_TRUE(tag); // validate our synchro point worked

  tag = false;
  { // This scope is a synchro point to avoid tests running in //
    qi::Promise<int> p;
    qi::FutureSync<int> syncPoint = p.future();
    {
      qi::FutureSync<int> fs;
      fs = p.future();
      qi::Future<int> fa = fs;
      eventLoop->async(boost::bind(unlock, p, &tag), 50000);
    }
    ASSERT_FALSE(tag); // fs was copied: blocking disabled
  }
  ASSERT_TRUE(tag); // validate our synchro point worked

  ASSERT_TRUE(true);
}

qi::FutureSync<int> getSync(bool* tag)
{
  qi::EventLoop* el = qi::getDefaultObjectEventLoop();
  qi::Promise<int> promise;
  el->async(boost::bind(unlock, promise, tag), 50000);
  return promise.future();
}

qi::FutureSync<int> getSync2(bool* tag)
{
  qi::EventLoop* el = qi::getDefaultObjectEventLoop();
  qi::Promise<int> promise;
  el->async(boost::bind(unlock, promise, tag), 50000);
  return promise.future().sync();
}

qi::FutureSync<int> getGetSync(bool* tag)
{
  return getSync(tag);
}

qi::FutureSync<int> getGetSync2(bool* tag)
{
  return getSync2(tag);
}

TEST(TestFutureSync, InSitu)
{
  /* Check that whatever we do, a function returning a FutureSync is not
  * stuck if we take the sync, and blocks if we ignore it
  */
  bool tag = false;
  {
    qi::FutureSync<int> fs = getSync(&tag);
    ASSERT_FALSE(tag);
  }
  ASSERT_TRUE(tag);
  tag = false;
  {
    qi::FutureSync<int> fs = getSync2(&tag);
    ASSERT_FALSE(tag);
  }
  ASSERT_TRUE(tag);
  tag = false;
  {
    qi::FutureSync<int> fs = getGetSync(&tag);
    ASSERT_FALSE(tag);
  }
  ASSERT_TRUE(tag);
  tag = false;
  {
    qi::FutureSync<int> fs = getGetSync2(&tag);
    ASSERT_FALSE(tag);
  }
  ASSERT_TRUE(tag);
  tag = false;
  {
    getSync(&tag);
    ASSERT_TRUE(tag);
  }
  ASSERT_TRUE(tag);
  tag = false;
  {
    getSync2(&tag);
    ASSERT_TRUE(tag);
  }
  ASSERT_TRUE(tag);
  tag = false;
}

TEST(TestFutureSync, ThrowOnDestroy) {
  qi::Promise<int> prom;

  prom.setError("touctouc");
  EXPECT_THROW(qi::FutureSync<int>(prom.future()), std::runtime_error);
  EXPECT_THROW(qi::FutureSync<int>(prom.future()), std::runtime_error); //multiple futuresync on the same future should fail.
  EXPECT_THROW(qi::FutureSync<int>(prom.future()).value(), std::runtime_error); //value should fail.
}

TEST(TestFutureSync, NoThrow) {
  qi::Promise<int> prom;

  prom.setError("touctouc");
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).wait(qi::FutureTimeout_None));
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).hasError());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).hasValue());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).isRunning());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).isCanceled());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).isFinished());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).error());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).cancel());
  EXPECT_NO_THROW(qi::FutureSync<int>(prom.future()).isCanceleable());
}

void do_nothing(TestFutureI*) {}

TEST(TestFutureError, MultipleSetValue)
{
  qi::Promise<int> p;
  qi::Future<int> f = p.future();
  p.setValue(0);
  p.reset();
  p.setError("");
  p.reset();
  p.setValue(1);
  EXPECT_ANY_THROW({ p.setValue(0);});
}

TEST(TestFutureError, ValueOnError)
{
  qi::Promise<int> p;
  qi::Future<int> f = p.future();
  p.setError("foo");
  EXPECT_ANY_THROW({ f.value();});
}


TEST(TestFutureCancel, NotCanceleable)
{
  qi::Promise<int> p;
  qi::Future<int> f = p.future();
  EXPECT_ANY_THROW({f.cancel();});
}

static void setTrue(bool* b)
{
  *b = true;
}

TEST(TestFutureCancel, AsyncCallCanceleable)
{
  bool b = false;
  qi::Future<void> f = qi::getDefaultNetworkEventLoop()->async(
    boost::bind(&setTrue, &b), 200);
  f.cancel();
  ASSERT_FALSE(f.isFinished());
  qi::os::msleep(400);
  ASSERT_TRUE(!b);
  ASSERT_TRUE(f.isFinished());
  ASSERT_TRUE(f.hasError());
}

static void doCancel(qi::Promise<int> promise)  { promise.setCanceled(); }
static void doError(qi::Promise<int> promise)   { promise.setError("paf");}
static void doValue(qi::Promise<int> promise)   { promise.setValue(42); }
static void doNothing(qi::Promise<int> promise) { ; }

TEST(TestFutureCancel, Canceleable)
{
  qi::Promise<int> p(doCancel);
  qi::Future<int> f = p.future();

  ASSERT_FALSE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  f.cancel();
  ASSERT_TRUE(f.isFinished());
  ASSERT_TRUE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  ASSERT_FALSE(f.hasError(qi::FutureTimeout_None));
  ASSERT_FALSE(f.hasValue(qi::FutureTimeout_None));


  p = qi::Promise<int>(doError);
  f = p.future();

  ASSERT_FALSE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  f.cancel();
  ASSERT_TRUE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  ASSERT_TRUE(f.hasError(qi::FutureTimeout_None));
  ASSERT_FALSE(f.hasValue(qi::FutureTimeout_None));

  p = qi::Promise<int>(doValue);
  f = p.future();

  ASSERT_FALSE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  f.cancel();
  ASSERT_TRUE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  ASSERT_FALSE(f.hasError(qi::FutureTimeout_None));
  ASSERT_TRUE(f.hasValue(qi::FutureTimeout_None));

  p = qi::Promise<int>(doNothing);
  f = p.future();

  ASSERT_FALSE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  f.cancel();
  ASSERT_FALSE(f.isFinished());
  ASSERT_FALSE(f.isCanceled());
  ASSERT_TRUE(f.isCanceleable());
  EXPECT_THROW(f.hasError(qi::FutureTimeout_None), qi::FutureException);
  EXPECT_THROW(f.hasValue(qi::FutureTimeout_None), qi::FutureException);
}


TEST(TestFutureCancel, Canceled)
{
  bool b = false;
  qi::Future<void> f = qi::getDefaultNetworkEventLoop()->async(
    boost::bind(&setTrue, &b), 200);
  f.cancel();
  ASSERT_FALSE(f.isFinished());
  qi::os::msleep(400);
  ASSERT_TRUE(!b);
  ASSERT_TRUE(f.isFinished());
  ASSERT_TRUE(f.hasError());
}


// ===== FutureBarrier =========================================================
#define BARRIER_N 10

static void addMe(qi::Atomic<int>* a) {
  ++(*a);
}

static void mult42(qi::Promise<int> prom, int number) {
  prom.setValue(42 * number);
}

static void checkBarrier(qi::Promise<void> prom,
                         std::vector< qi::Future<int> > results)
{
  for (int it = 0; it < BARRIER_N; ++it) {
    qi::Future<int> fut = results[it];

    ASSERT_FALSE(fut.hasError());
    ASSERT_EQ(it * 42, fut.value());
  }
  prom.setValue(0);
}

TEST(TestPromiseBarrier, SimpleBarrier)
{
  int it;
  qi::Atomic<int> a;
  qi::FutureBarrier<void> barrier;
  qi::Promise<void> call;

  // Loading the barrier.
  for (it = 0; it < BARRIER_N; ++it) {
    // function call in reality here.
    call.future().connect(boost::bind<void>(&addMe, &a));
    barrier.addFuture(call.future());
  }

  // Launch all addMe (just think of it like function calls returned)
  call.setValue(0);

  // We wait for all futures of the for loop.
  barrier.future().wait();
  ASSERT_EQ(it, *a);
}

TEST(TestPromiseBarrier, ClosedBarrier)
{
  qi::FutureBarrier<void> barrier;

  // Can add a future to the barrier because is not yet closed.
  qi::Promise<void> prom;
  ASSERT_TRUE(barrier.addFuture(prom.future()));
  prom.setValue(0);

  barrier.future().wait();

  // Invalid promise, because FutureBarrier is closed.
  qi::Promise<void> prom2;
  ASSERT_FALSE(barrier.addFuture(prom2.future()));
}

TEST(TestPromiseBarrier, CompleteExample)
{
  qi::Promise<void> call;
  qi::FutureBarrier<int> barrier;

  // Load data in the barrier.
  for (int it = 0; it < BARRIER_N; ++it) {
    qi::Promise<int> prom;
    call.future().connect(boost::bind<void>(&mult42, prom, it));
    barrier.addFuture(prom.future());
  }
  call.setValue(0);

  // Bind something to do after everything is computed.
  qi::Promise<void> end;
  barrier.future().connect(boost::bind(&checkBarrier, end, _1));

  // Wait for the end of the check.
  end.future().wait();
}


qi::Future<int> emulateSet(int it, bool error = false) {
  qi::Promise<int> prom;

  if (error) {
    prom.setError("ERROR");
  } else {
    prom.setValue(it);
  }
  return prom.future();
}

TEST(TestWaitForAll, SimpleTest) {
  std::vector< qi::Future<int> > vect;

  for (int it = 0; it < BARRIER_N; ++it) {
    vect.push_back(emulateSet(it));
  }
  qi::waitForAll<int>(vect);
  for (int it = 0; it < BARRIER_N; ++it) {
    ASSERT_FALSE(vect.at(it).hasError());
    ASSERT_EQ(vect.at(it).value(), it);
  }
}

TEST(TestWaitForFirst, SuccessfulTest) {
  std::vector< qi::Future<int> > vect;

  for (int it = 0; it < BARRIER_N; ++it) {
    vect.push_back(emulateSet(it, it != 3));
  }
  qi::Future<int> a = qi::waitForFirst<int>(vect);
  ASSERT_FALSE(a.hasError());
  ASSERT_EQ(a.value(), 3);
}

TEST(TestWaitForFirst, FailingTest) {
  std::vector< qi::Future<int> > vect;

  for (int it = 0; it < BARRIER_N; ++it) {
    vect.push_back(emulateSet(it, true));
  }
  qi::Future<int> a = qi::waitForFirst<int>(vect);
  ASSERT_TRUE(a.hasError());
}

int main(int argc, char **argv) {
  qi::Application app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
