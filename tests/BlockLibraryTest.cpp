#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/BlockLibrary.h"
#include "logicbench/CircuitSerializer.h"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace lb;
namespace fs = std::filesystem;

namespace {

/// A fresh, empty folder per test, removed afterwards.
class BlockLibraryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_dir = fs::temp_directory_path() / ("lb_blocks_" + std::to_string(stamp));
    }
    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(m_dir, ec);
    }

    Circuit halfAdder() const
    {
        Circuit c;
        lbtest::buildHalfAdder(c);
        return c;
    }

    fs::path m_dir;
};

} // namespace

TEST_F(BlockLibraryTest, AnEmptyOrMissingFolderListsNothing)
{
    BlockLibrary library(m_dir);
    EXPECT_TRUE(library.list().empty());
    EXPECT_FALSE(fs::exists(m_dir));                 // created on first write, not on read
}

TEST_F(BlockLibraryTest, AddListsTheBlockWithItsPinsAndContents)
{
    BlockLibrary library(m_dir);
    const fs::path path = library.add(halfAdder(), "half_adder");
    EXPECT_TRUE(fs::exists(path));
    EXPECT_EQ(path.filename(), "half_adder.lbc");

    const std::vector<BlockInfo> blocks = library.list();
    ASSERT_EQ(blocks.size(), 1u);
    EXPECT_EQ(blocks[0].name, "half_adder");
    EXPECT_EQ(blocks[0].inputs, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(blocks[0].outputs, (std::vector<std::string>{"C", "S"}));
    EXPECT_EQ(blocks[0].contents, "1 XOR gate, 1 AND gate");
}

TEST_F(BlockLibraryTest, ABlockSurvivesARestart)
{
    BlockLibrary(m_dir).add(halfAdder(), "half_adder");
    BlockLibrary later(m_dir);                       // a new session
    ASSERT_TRUE(later.contains("half_adder"));
    std::unique_ptr<SubCircuit> block = later.load("half_adder");
    EXPECT_EQ(block->blockName(), "half_adder");
    EXPECT_EQ(block->pinCount(), 4u);
}

TEST_F(BlockLibraryTest, RefusesBadNamesTakenNamesAndBlocksWithoutPins)
{
    BlockLibrary library(m_dir);
    library.add(halfAdder(), "half_adder");
    EXPECT_THROW(library.add(halfAdder(), "half_adder"), BlockLibraryError);
    EXPECT_THROW(library.add(halfAdder(), "../escape"), BlockLibraryError);
    EXPECT_THROW(library.add(halfAdder(), ""), BlockLibraryError);
    EXPECT_THROW(library.add(Circuit(), "empty"), BlockLibraryError);
}

TEST_F(BlockLibraryTest, RenameAndDelete)
{
    BlockLibrary library(m_dir);
    library.add(halfAdder(), "ha");
    library.rename("ha", "half adder");
    EXPECT_FALSE(library.contains("ha"));
    EXPECT_TRUE(library.contains("half adder"));
    EXPECT_THROW(library.rename("missing", "x"), BlockLibraryError);

    library.add(halfAdder(), "other");
    EXPECT_THROW(library.rename("other", "half adder"), BlockLibraryError);

    library.remove("half adder");
    EXPECT_FALSE(library.contains("half adder"));
    EXPECT_EQ(library.list().size(), 1u);
    EXPECT_THROW(library.remove("half adder"), BlockLibraryError);
}

TEST_F(BlockLibraryTest, ImportCopiesAFileAndAvoidsNameClashes)
{
    fs::create_directories(m_dir);
    const fs::path outside = m_dir / "outside.lbc.src";
    CircuitSerializer::save(halfAdder(), outside.string());
    const fs::path named = m_dir.parent_path() / ("lb_import_" + m_dir.filename().string() + ".lbc");
    fs::copy_file(outside, named, fs::copy_options::overwrite_existing);

    BlockLibrary library(m_dir / "lib");
    const std::string first = library.importFile(named);
    const std::string second = library.importFile(named);
    EXPECT_NE(first, second);
    EXPECT_TRUE(library.contains(first));
    EXPECT_TRUE(library.contains(second));
    fs::remove(named);
}

TEST_F(BlockLibraryTest, ImportingSomethingThatIsNotACircuitFails)
{
    fs::create_directories(m_dir);
    const fs::path junk = m_dir / "junk.lbc";
    std::ofstream(junk) << "not json";
    BlockLibrary library(m_dir / "lib");
    EXPECT_THROW(library.importFile(junk), BlockLibraryError);
}

TEST_F(BlockLibraryTest, AnUnreadableFileIsSkippedNotFatal)
{
    BlockLibrary library(m_dir);
    library.add(halfAdder(), "good");
    std::ofstream(m_dir / "broken.lbc") << "{ nope";
    const std::vector<BlockInfo> blocks = library.list();
    ASSERT_EQ(blocks.size(), 1u);
    EXPECT_EQ(blocks[0].name, "good");
}

TEST_F(BlockLibraryTest, UsersOfFindsEveryBlockThatContainsIt)
{
    BlockLibrary library(m_dir);
    library.add(halfAdder(), "half_adder");

    // full_adder holds a half_adder block; adder2 holds a full_adder block,
    // so it uses half_adder one level deeper.
    Circuit full = halfAdder();
    full.add(library.load("half_adder"));
    library.add(full, "full_adder");

    Circuit adder2 = halfAdder();
    adder2.add(library.load("full_adder"));
    library.add(adder2, "adder2");

    library.add(halfAdder(), "unrelated");

    const std::vector<std::string> users = library.usersOf("half_adder");
    EXPECT_EQ(users, (std::vector<std::string>{"adder2", "full_adder"}));
    EXPECT_TRUE(library.usersOf("adder2").empty());
}

TEST_F(BlockLibraryTest, AFailedWriteLeavesNoPartialFile)
{
    BlockLibrary library(m_dir);
    library.add(halfAdder(), "half_adder");
    for (const auto& entry : fs::directory_iterator(m_dir))
        EXPECT_NE(entry.path().extension(), ".part");
}
