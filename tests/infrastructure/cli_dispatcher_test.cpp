#include <gtest/gtest.h>
#include "infrastructure/cli_dispatcher.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace nwave;

class CliDispatcherTest : public ::testing::Test {
protected:
    std::ostringstream out;
    std::ostringstream err;

    // Builds argv-style array from strings. Caller must keep `args` alive.
    static std::vector<char*> make_argv(std::vector<std::string>& args) {
        std::vector<char*> argv;
        for (auto& a : args) {
            argv.push_back(a.data());
        }
        return argv;
    }

    CliDispatcher make_dispatcher() {
        return CliDispatcher(out, err);
    }

    int dispatch(std::vector<std::string> args) {
        auto argv = make_argv(args);
        int argc = static_cast<int>(argv.size());
        auto dispatcher = make_dispatcher();
        return dispatcher.dispatch(argc, argv.data());
    }
};

// --- Acceptance test: render subcommand parses file and flags correctly ---

TEST_F(CliDispatcherTest, RenderSubcommandParsesFileAndOverrideFlags) {
    // Wire a test render handler that captures the parsed command
    RenderCommand captured;
    bool handler_called = false;

    auto dispatcher = make_dispatcher();
    dispatcher.set_render_handler([&](const RenderCommand& cmd) {
        captured = cmd;
        handler_called = true;
        return 0;
    });

    std::vector<std::string> args = {"nwave", "render", "scene.yaml",
                                     "--width", "400", "--spp", "4",
                                     "-o", "out.ppm"};
    auto argv = make_argv(args);
    int rc = dispatcher.dispatch(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(rc, 0);
    ASSERT_TRUE(handler_called);
    EXPECT_EQ(captured.scene_file, "scene.yaml");
    EXPECT_EQ(captured.width, 400);
    EXPECT_EQ(captured.spp, 4);
    EXPECT_EQ(captured.output, "out.ppm");
}

// --- Unit tests for argument parsing behaviors ---

TEST_F(CliDispatcherTest, NoArgumentsPrintsUsageAndReturnsZero) {
    int rc = dispatch({"nwave"});

    std::string output = out.str();
    EXPECT_NE(output.find("Usage:"), std::string::npos);
    EXPECT_NE(output.find("render"), std::string::npos);
    EXPECT_EQ(rc, 0);
}

TEST_F(CliDispatcherTest, HelpFlagPrintsUsageAndReturnsZero) {
    int rc = dispatch({"nwave", "--help"});

    std::string output = out.str();
    EXPECT_NE(output.find("Usage:"), std::string::npos);
    EXPECT_NE(output.find("render"), std::string::npos);
    EXPECT_EQ(rc, 0);
}

TEST_F(CliDispatcherTest, UnknownSubcommandPrintsHelpAndReturnsNonZero) {
    int rc = dispatch({"nwave", "bogus"});

    std::string error_output = err.str();
    EXPECT_NE(error_output.find("Unknown"), std::string::npos);
    std::string combined = out.str() + err.str();
    EXPECT_NE(combined.find("Usage:"), std::string::npos);
    EXPECT_NE(rc, 0);
}

TEST_F(CliDispatcherTest, RenderSubcommandWithoutFilePrintsErrorAndReturnsNonZero) {
    int rc = dispatch({"nwave", "render"});

    std::string error_output = err.str();
    EXPECT_NE(error_output.find("file"), std::string::npos);
    EXPECT_NE(rc, 0);
}

TEST_F(CliDispatcherTest, RenderSubcommandUsesDefaultOutputFilename) {
    RenderCommand captured;
    auto dispatcher = make_dispatcher();
    dispatcher.set_render_handler([&](const RenderCommand& cmd) {
        captured = cmd;
        return 0;
    });

    std::vector<std::string> args = {"nwave", "render", "scene.yaml"};
    auto argv = make_argv(args);
    dispatcher.dispatch(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(captured.output, "output.ppm");
    EXPECT_EQ(captured.width, 0);
    EXPECT_EQ(captured.spp, 0);
}

// --- Acceptance test: validate subcommand dispatches to validate handler ---

TEST_F(CliDispatcherTest, ValidateSubcommandDispatchesToHandlerAndReturnsExitCode) {
    ValidateCommand captured;
    bool handler_called = false;

    auto dispatcher = make_dispatcher();
    dispatcher.set_validate_handler([&](const ValidateCommand& cmd) {
        captured = cmd;
        handler_called = true;
        return 0;  // valid scene
    });

    std::vector<std::string> args = {"nwave", "validate", "scene.yaml"};
    auto argv = make_argv(args);
    int rc = dispatcher.dispatch(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(rc, 0);
    ASSERT_TRUE(handler_called);
    EXPECT_EQ(captured.scene_file, "scene.yaml");
}

TEST_F(CliDispatcherTest, ValidateSubcommandWithoutFilePrintsErrorAndReturnsNonZero) {
    int rc = dispatch({"nwave", "validate"});

    std::string error_output = err.str();
    EXPECT_NE(error_output.find("file"), std::string::npos);
    EXPECT_NE(rc, 0);
}

TEST_F(CliDispatcherTest, ValidateSubcommandPropagatesNonZeroExitCode) {
    auto dispatcher = make_dispatcher();
    dispatcher.set_validate_handler([&](const ValidateCommand&) {
        return 2;  // invalid scene
    });

    std::vector<std::string> args = {"nwave", "validate", "bad_scene.yaml"};
    auto argv = make_argv(args);
    int rc = dispatcher.dispatch(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(rc, 2);
}

TEST_F(CliDispatcherTest, UsageTextIncludesValidateSubcommand) {
    int rc = dispatch({"nwave", "--help"});

    std::string output = out.str();
    EXPECT_NE(output.find("validate"), std::string::npos);
    EXPECT_EQ(rc, 0);
}

TEST_F(CliDispatcherTest, AnimateFlagPreservesLegacyBehavior) {
    bool legacy_called = false;
    auto dispatcher = make_dispatcher();
    dispatcher.set_legacy_handler([&]() {
        legacy_called = true;
        return 0;
    });

    std::vector<std::string> args = {"nwave", "--animate"};
    auto argv = make_argv(args);
    int rc = dispatcher.dispatch(static_cast<int>(argv.size()), argv.data());

    std::string output = out.str();
    std::string error_output = err.str();
    EXPECT_EQ(output.find("Usage:"), std::string::npos);
    EXPECT_EQ(error_output.find("Unknown"), std::string::npos);
    EXPECT_TRUE(legacy_called);
    EXPECT_EQ(rc, 0);
}
