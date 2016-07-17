/* srccpy - A C source-to-source translator, implemented with libtooling.
 *
 * ----
 *
 * Clang is a demanding code base. Its repositories undergo a lot of churn and many of its internal
 * APIs are not stable. The following code was written based on the tool template from revision
 * 3979e83d5a51f469186e5e23c31284a070791e23. If it does not build, it likely means the libtooling
 * API has changed. Checking the changes between that revision and the current tip may provide some
 * clues as to how to proceed.
 */

#include <string>
#include <unordered_map>

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Signals.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

// LLVM boiler plate
class Callback : public MatchFinder::MatchCallback {
 public:
  Callback(Replacements *Replace) : m_replace(Replace) {}

  void run(const MatchFinder::MatchResult &Result) override;

 private:
  Replacements *m_replace;
};

// Set up the command line options
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp SrccpyHelp("a C source rewriter\n");
static cl::OptionCategory Category("srccpy options");

// Extra command line options
static cl::opt<bool> Verbose("verbose", cl::desc("Output additional information"),
  cl::cat(Category), cl::ZeroOrMore);
static cl::alias verbose1("v", cl::desc("Alias for -verbose"), cl::aliasopt(Verbose));
static cl::opt<string> Output("output", cl::desc("Output file (defaults to stdout)"),
  cl::cat(Category));
static cl::alias output1("o", cl::desc("Alias for -output"), cl::aliasopt(Output));
static cl::list<string> Rename("rename",
  cl::desc("Rename a function or global variable (use as -rename=oldname=newname)"),
  cl::cat(Category));

static unordered_map<string, string> renames;

int main(int argc, const char **argv) {

  // Boiler plate from libtooling template
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  CommonOptionsParser OptionsParser(argc, argv, Category);
  RefactoringTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  ast_matchers::MatchFinder Finder;
  Callback cb(&Tool.getReplacements());

  if (OptionsParser.getSourcePathList().size() != 1) {
    errs() << "this tool only supports a single input file\n";
    return EXIT_FAILURE;
  }

  // Set up all renames
  for (auto r : Rename) {
    size_t equals = r.find('=');
    if (equals == string::npos) {
      llvm::errs() << "malformed argument -rename=" << r << "\n";
      return EXIT_FAILURE;
    }
    string oldname(r, 0, equals),
                newname(r, equals + 1);
    renames[oldname] = newname;
    // Match definitions (including prototypes) of this function
    DeclarationMatcher matcher = functionDecl(hasName(oldname)).bind("func");
    Finder.addMatcher(matcher, &cb);
    // Match calls to this function
    StatementMatcher matcher2 = callExpr(callee(functionDecl(hasName(oldname)))).bind("func");
    Finder.addMatcher(matcher2, &cb);
  }

  // Scan the input file, accumulating rewrites
  int ret = Tool.run(newFrontendActionFactory(&Finder).get());
  if (ret != 0)
    return EXIT_FAILURE;

  if (Verbose) {
    errs() << "Rewrites:\n";
    for (auto r : Tool.getReplacements()) {
      errs() << r.toString() << "\n";
    }
  }

  // The following lifted from the Clang-rename tool
  LangOptions DefaultLangOptions;
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter DiagnosticPrinter(errs(), &*DiagOpts);
  DiagnosticsEngine Diagnostics(IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()),
    &*DiagOpts, &DiagnosticPrinter, false);
  auto &FileMgr = Tool.getFiles();
  SourceManager Sources(Diagnostics, FileMgr);
  Rewriter Rewrite(Sources, DefaultLangOptions);

  // Write out the replacements to either stdout or the output file
  Tool.applyAllReplacements(Rewrite);
  const auto File = OptionsParser.getSourcePathList()[0];
  const auto *Entry = FileMgr.getFile(File);
  auto ID = Sources.translateFile(Entry);
  if (Output.getNumOccurrences()) {
    error_code ec;
    raw_fd_ostream out(StringRef(Output), ec, llvm::sys::fs::F_None);
    if (ec) {
      errs() << "failed to open " << Output << "\n";
      return EXIT_FAILURE;
    }
    Rewrite.getEditBuffer(ID).write(out);
  } else {
    Rewrite.getEditBuffer(ID).write(outs());
  }

  return EXIT_SUCCESS;
}

// Where we connect matched expressions to replacements
void Callback::run(const MatchFinder::MatchResult &Result) {
  if (const FunctionDecl *f = Result.Nodes.getNodeAs<FunctionDecl>("func")) {
    auto nameinfo = f->getNameInfo();
    string name = nameinfo.getAsString();
    auto newname = renames.find(name);
    if (newname != renames.end()) {
      SourceLocation start = nameinfo.getBeginLoc();
      unsigned length = name.length();
      Replacement rep(*Result.SourceManager, start, length, llvm::StringRef(newname->second.c_str()));
      m_replace->insert(rep);
    }
  } else if (const CallExpr *c = Result.Nodes.getNodeAs<CallExpr>("func")) {
    const FunctionDecl *f = c->getDirectCallee();
    if (f != nullptr) {
      auto nameinfo = f->getNameInfo();
      string name = nameinfo.getAsString();
      auto newname = renames.find(name);
      if (newname != renames.end()) {
        SourceLocation start = c->getLocStart();
        unsigned length = name.length();
        Replacement rep(*Result.SourceManager, start, length, llvm::StringRef(newname->second.c_str()));
        m_replace->insert(rep);
      }
    }
  }
}
