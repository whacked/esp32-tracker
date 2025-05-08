#include <gtest/gtest.h>
#include "../src/CommandHandler.h"
#include "../src/DataLogger.h"
#include "../src/generated/cpp_bt_commands_codegen.h"
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
    auto [cmd, args] = CommandHandler::parseCommand(CMD_GETVERSION);
    EXPECT_EQ(handler.handleCommand(cmd, args), "0.0." + std::to_string(BUILD_NUMBER));
}

TEST_F(CommandHandlerTest, SetTime)
{
    time_t now = time(nullptr);
    auto [cmd, args] = CommandHandler::parseCommand(
        std::string(CMD_SETTIME) + " " + std::to_string(now));
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"ok\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"offset\":0") != std::string::npos);
}

TEST_F(CommandHandlerTest, ClearBuffer)
{
    auto [cmd, args] = CommandHandler::parseCommand(CMD_CLEARBUFFER);
    EXPECT_EQ(handler.handleCommand(cmd, args), "{\"status\":\"ok\"}");
    EXPECT_EQ(dataLogger.getBufferSize(), 0);
}

TEST_F(CommandHandlerTest, ReadBuffer)
{
    // Add some test data
    dataLogger.addRecord(1, 2, 3.0f, MEASUREMENT);
    dataLogger.addRecord(4, 5, 6.0f, REFILL);
    dataLogger.addRecord(7, 8, 9.0f, SIP);

    auto [cmd, args] = CommandHandler::parseCommand(std::string(CMD_READBUFFER) + " 1 2");
    std::string response = handler.handleCommand(cmd, args);
    std::cout << "response: " << response << std::endl;
    EXPECT_EQ(response, "{\"length\":2,\"records\":[{\"start_time\":4,\"end_time\":5,\"grams\":6.000000,\"type\":\"refill\"},{\"start_time\":7,\"end_time\":8,\"grams\":9.000000,\"type\":\"sip\"}]}");
}

TEST_F(CommandHandlerTest, StartStopLogging)
{
    auto [startCmd, startArgs] = CommandHandler::parseCommand(CMD_STARTLOGGING);
    EXPECT_EQ(handler.handleCommand(startCmd, startArgs), "{\"status\":\"ok\"}");
    EXPECT_TRUE(dataLogger.isLoggingEnabled());

    auto [stopCmd, stopArgs] = CommandHandler::parseCommand(CMD_STOPLOGGING);
    EXPECT_EQ(handler.handleCommand(stopCmd, stopArgs), "{\"status\":\"ok\"}");
    EXPECT_FALSE(dataLogger.isLoggingEnabled());
}

TEST_F(CommandHandlerTest, GetNow)
{
    auto [cmd, args] = CommandHandler::parseCommand(CMD_GETNOW);
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"epoch\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"local\":") != std::string::npos);
}

TEST_F(CommandHandlerTest, GetStatus)
{
    auto [cmd, args] = CommandHandler::parseCommand(CMD_GETSTATUS);
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"logging\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"bufferSize\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"rateHz\":10") != std::string::npos);
}

TEST_F(CommandHandlerTest, SetSamplingRate)
{
    auto [cmd, args] = CommandHandler::parseCommand(std::string(CMD_SETSAMPLINGRATE) + " 20");
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

    auto [cmd, args] = CommandHandler::parseCommand(std::string(CMD_DROPRECORDS) + " 1 1");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"ok\"") != std::string::npos);
    EXPECT_EQ(dataLogger.getBufferSize(), 2);
}

TEST_F(CommandHandlerTest, UnknownCommand)
{
    auto [cmd, args] = CommandHandler::parseCommand("blah-blah-unknown-command");
    std::string response = handler.handleCommand(cmd, args);
    EXPECT_TRUE(response.find("\"status\":\"error\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"message\":\"Unknown command: 'blah-blah-unknown-command'\"") != std::string::npos);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}