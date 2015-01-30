// Copyright 2013
// Author: Christopher Van Arsdale

#include <map>
#include <set>
#include <string>
#include <vector>
#include "common/base/flags.h"
#include "common/log/log.h"
#include "common/strings/path.h"
#include "common/strings/strutil.h"
#include "repobuild/env/input.h"
#include "repobuild/nodes/gen_sh.h"
#include "repobuild/nodes/makefile.h"
#include "repobuild/nodes/util.h"
#include "repobuild/reader/buildfile.h"

DEFINE_bool(silent_gensh, true,
            "If true, we only print out gen_sh stderr/stdout on "
            "script failures.");

using std::map;
using std::string;
using std::vector;
using std::set;

namespace repobuild {
namespace {
const char kRootDir[] = "ROOT_DIR";
}

void GenShNode::Parse(BuildFile* file, const BuildFileNode& input) {
  Node::Parse(file, input);
  if (!current_reader()->ParseStringField("build_cmd", cd_, &build_cmd_) &&
      !current_reader()->ParseStringField("cmd", cd_, &build_cmd_)) {
    LOG(FATAL) << "Could not parse build_cmd/cmd.";
  }
  current_reader()->ParseStringField("clean", false, &clean_cmd_);
  current_reader()->ParseRepeatedFiles("input_files", &input_files_);
  current_reader()->ParseRepeatedFiles("outs", false, &outputs_);
}

void GenShNode::Set(const string& build_cmd,
                    const string& clean_cmd,
                    const vector<Resource>& input_files,
                    const vector<Resource>& outputs) {
  build_cmd_ = build_cmd;
  clean_cmd_ = clean_cmd;
  input_files_ = input_files;
  outputs_ = outputs;
}

// static
void GenShNode::WriteMakeHead(const Input& input, Makefile* out) {
  out->append("# Environment flag settings.\n");
  out->append(string(kRootDir) + " := $(shell pwd)\n");
}

void GenShNode::LocalWriteMake(Makefile* out) const {
  Resource touchfile = Touchfile();

  // Inputs
  ResourceFileSet input_files;
  InputDependencyFiles(NO_LANG, &input_files);  // all but our own.

  ResourceFileSet obj_files;
  ObjectFiles(NO_LANG, &obj_files);

  // Make target
  Makefile::Rule* rule = out->StartRule(touchfile.path(), strings::JoinWith(
      " ",
      strings::JoinAll(input_files.files(), " "),
      strings::JoinAll(obj_files.files(), " "),
      strings::JoinAll(input_files_, " ")));

  // Build command.
  if (!build_cmd_.empty()) {
    rule->WriteUserEcho(make_name_, make_target_);

    // The file we touch after the script runs, for 'make' to be happy.
    string touch_cmd = "mkdir -p " +
        strings::JoinPath(input().object_dir(), target().dir()) +
        "; touch " + touchfile.path();

    // Compute the build command prefix.
    string prefix;
    {
      set<string> compile_flags;
      CompileFlags(CPP, &compile_flags);
      prefix = "DEP_CXXFLAGS=\"" + strings::JoinAll(compile_flags, " ") + "\"";
      compile_flags.clear();
      CompileFlags(C_LANG, &compile_flags);
      prefix += " DEP_CFLAGS=\"" + strings::JoinAll(compile_flags, " ") + "\"";
    }

    // Compute environment variables for shell.
    map<string, string> env_vars;
    EnvVariables(NO_LANG, &env_vars);
    for (auto it : local_env_vars_) {  // local vars override inherited ones.
      env_vars[it.first] = it.second;
    }

    // Now write the actual comment.
    // This is a hack for now.
    string command = strings::ReplaceAll(
        build_cmd_, "$(ROOT_DIR)", "$ROOT_DIR");
    rule->WriteCommand(WriteCommand(env_vars, prefix, command, touch_cmd));
  }
  out->FinishRule(rule);

  {  // user target
    ResourceFileSet output_targets;
    output_targets.Add(touchfile);
    WriteBaseUserTarget(output_targets, out);
  }

  for (const Resource& resource : outputs_) {
    out->WriteRule(resource.path(), touchfile.path());
  }
}

void GenShNode::LocalWriteMakeClean(Makefile::Rule* rule) const {
  if (clean_cmd_.empty()) {
    return;
  }

  map<string, string> env_vars;
  EnvVariables(NO_LANG, &env_vars);
  rule->WriteCommandBestEffort(WriteCommand(env_vars, "", clean_cmd_, ""));
}

namespace {
void AddEnvVar(const string& var, string* out) {
  out->append(" ");
  out->append(var);
  out->append("=\"$(");
  out->append(var);
  out->append(")\"");
}
string JoinRoot(const string& path) {
  return strings::JoinPath("$(" + string(kRootDir) + ")/", path);
}
}

string GenShNode::WriteCommand(const map<string, string>& env_vars,
                               const string& prefix,
                               const string& cmd,
                               const string& admin_cmd) const {
  string out;
  out.append("(mkdir -p ");
  out.append(GenDir());
  if (cd_ && !target().dir().empty()) {
    out.append("; cd ");
    out.append(target().dir());
  }

  // Environment.
  out.append("; GEN_DIR=\"" + JoinRoot(GenDir()) + "\"");
  out.append("; OBJ_DIR=\"" + JoinRoot(ObjectDir()) + "\"");
  out.append("; SRC_DIR=\"" + JoinRoot(SourceDir()) + "\"");
  out.append("; PKG_DIR=\"" + JoinRoot(PackageDir()) + "\"");
  out.append(" " + string(kRootDir) + "=\"$(" + string(kRootDir) + ")\" ");
  AddEnvVar("CXX_GCC", &out);
  AddEnvVar("CC_GCC", &out);
  AddEnvVar("CC", &out);
  AddEnvVar("CXX", &out);
  AddEnvVar("CXXFLAGS", &out);
  AddEnvVar("BASIC_CXXFLAGS", &out);
  AddEnvVar("CFLAGS", &out);
  AddEnvVar("BASIC_CFLAGS", &out);
  AddEnvVar("LDFLAGS", &out);
  AddEnvVar("MAKE", &out);
  for (const auto& it : env_vars) {
    out.append(" ");
    out.append(it.first);
    out.append("=\"");
    out.append(it.second);
    out.append("\"");
  }
  out.append(" ");
  out.append(prefix);

  // TODO: Pass up header_compile_args from dependencies as DEP_FLAGS

  // Execute command
  out.append(" eval '(");
  if (escape_command_) {
    out.append(Makefile::Escape(cmd));
  } else {
    out.append(cmd);
  }
  out.append(")'");

  // Logfile, if any
  if (FLAGS_silent_gensh) {
    string logfile = JoinRoot(GenDir() + "." + target().local_path() +
                              ".logfile");
    out.append(" > " + logfile + " 2>&1 || (cat " + logfile + "; exit 1)");
  }

  out.append(" )");  // matches top "(".

  // Admin command, if any.
  if (!admin_cmd.empty()) {
    out.append(" && (");
    out.append(admin_cmd);
    out.append(")");
  }
  return out;
}

void GenShNode::LocalDependencyFiles(LanguageType lang,
                                     ResourceFileSet* files) const {
  files->Add(Touchfile());
}

}  // namespace repobuild
