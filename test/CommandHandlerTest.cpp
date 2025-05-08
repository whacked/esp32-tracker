#include <gtest/gtest.h>
#include "../src/CommandHandler.h"
#include "../src/DataLogger.h"
#include <sstream>

class CommandHandlerTest : public ::testing::Test
{
protected:
    DataLogger dataLogger;
    int samplingRateHz = 10;
    DataLoggerCommandHandler handler{dataLogger, samplingRateHz};
};

TEST_F(CommandHandlerTest, GetVersion)
{
    auto [cmd, args] = CommandHandler::parseCommand("getversion");
    EXPECT_EQ(handler.handleCommand(cmd, args), "0.0.1");
}

TEST_F(CommandHandlerTest, SetTime)
{
    time_t now = time(nullptr);
    auto [cmd, args] = CommandHandler::parseCommand("settime " + std::to_string(now));
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"ok\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"offset\":0") != std::string::npos);
}

TEST_F(CommandHandlerTest, ClearBuffer)
{
    auto [cmd, args] = CommandHandler::parseCommand("clearbuffer");
    EXPECT_EQ(handler.handleCommand(cmd, args), "{\"status\":\"ok\"}");
    EXPECT_EQ(dataLogger.getBufferSize(), 0);
}

TEST_F(CommandHandlerTest, ReadBuffer)
{
    // Add some test data
    dataLogger.addRecord(1, 2, 3.0f, MEASUREMENT);
    dataLogger.addRecord(4, 5, 6.0f, REFILL);
    dataLogger.addRecord(7, 8, 9.0f, SIP);

    auto [cmd, args] = CommandHandler::parseCommand("readbuffer");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"ok\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"total\":2") != std::string::npos);
}

TEST_F(CommandHandlerTest, StartStopLogging)
{
    auto [startCmd, startArgs] = CommandHandler::parseCommand("startlogging");
    EXPECT_EQ(handler.handleCommand(startCmd, startArgs), "{\"status\":\"ok\"}");
    EXPECT_TRUE(dataLogger.isLoggingEnabled());

    auto [stopCmd, stopArgs] = CommandHandler::parseCommand("stoplogging");
    EXPECT_EQ(handler.handleCommand(stopCmd, stopArgs), "{\"status\":\"ok\"}");
    EXPECT_FALSE(dataLogger.isLoggingEnabled());
}

TEST_F(CommandHandlerTest, GetNow)
{
    auto [cmd, args] = CommandHandler::parseCommand("getnow");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"epoch\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"local\":") != std::string::npos);
}

TEST_F(CommandHandlerTest, GetStatus)
{
    auto [cmd, args] = CommandHandler::parseCommand("getstatus");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"logging\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"bufferSize\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"rateHz\":10") != std::string::npos);
}

TEST_F(CommandHandlerTest, SetSamplingRate)
{
    auto [cmd, args] = CommandHandler::parseCommand("setsamplingrate 20");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"ok\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"rate\":20") != std::string::npos);
    EXPECT_EQ(samplingRateHz, 20);
}

TEST_F(CommandHandlerTest, DropRecords)
{
    // Add some test data
    dataLogger.addRecord(1, 2, 3.0f, MEASUREMENT);
    dataLogger.addRecord(4, 5, 6.0f, REFILL);
    dataLogger.addRecord(7, 8, 9.0f, SIP);

    auto [cmd, args] = CommandHandler::parseCommand("droprecords 1 1");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"ok\"") != std::string::npos);
    EXPECT_EQ(dataLogger.getBufferSize(), 2);
}

TEST_F(CommandHandlerTest, UnknownCommand)
{
    auto [cmd, args] = CommandHandler::parseCommand("unknowncommand");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"error\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"message\":\"Unknown command\"") != std::string::npos);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}