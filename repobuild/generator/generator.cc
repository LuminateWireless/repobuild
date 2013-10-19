// Copyright 2013
// Author: Christopher Van Arsdale

#include <iostream>
#include <set>
#include <vector>
#include <string>
#include "common/log/log.h"
#include "common/strings/strutil.h"
#include "common/util/stl.h"
#include "repobuild/distsource/dist_source.h"
#include "repobuild/env/input.h"
#include "repobuild/env/resource.h"
#include "repobuild/generator/generator.h"
#include "repobuild/nodes/allnodes.h"
#include "repobuild/nodes/node.h"
#include "repobuild/reader/parser.h"

using std::string;
using std::vector;
using std::set;

namespace repobuild {
namespace {

void ExpandNode(const Parser& parser,
                const Node* node,
                set<const Node*>* parents,
                set<const Node*>* seen,
                vector<const Node*>* to_process) {
  if (!seen->insert(node).second) {
    // Already processed.
    return;
  }

  // Check for recursive dependency
  if (!parents->insert(node).second) {
    LOG(FATAL) << "Recursive dependency: " << node->target().full_path();
  }

  // Now expand our sub-node dependencies.
  for (const Node* dep : node->dependencies()) {
    ExpandNode(parser, dep, parents, seen, to_process);
  }

  // And record this node as ready to go in the queue.
  parents->erase(node);
  to_process->push_back(node);
}

}  // anonymous namespace

Generator::Generator(DistSource* source)
    : source_(source) {
}

Generator::~Generator() {
}

string Generator::GenerateMakefile(const Input& input) {
  // Our set of node types (cc_library, etc.).
  NodeBuilderSet builder_set;

  // Initialize makefile.
  Makefile out(input.root_dir(), input.genfile_dir());
  out.SetSilent(input.silent_make());
  out.append("# Auto-generated by repobuild, do not modify directly.\n\n");
  builder_set.WriteMakeHead(input, &out);
  source_->WriteMakeHead(input, &out);

  // Get our input tree of nodes.
  repobuild::Parser parser(&builder_set, source_);
  parser.Parse(input);

  // Figure out the order we want to write in our Makefile.
  set<const Node*> parents, seen;
  vector<const Node*> process_order;
  for (const Node* node : parser.input_nodes()) {
    ExpandNode(parser, node, &parents, &seen, &process_order);
  }

  std::cout << "Generating: Makefile" << std::endl;

  // Generate the makefile.
  for (const Node* node : process_order) {
    VLOG(1) << "Writing make: " << node->target().full_path();
    node->WriteMake(&out);
  }

  // Finish up node make files
  builder_set.FinishMakeFile(input, process_order, source_, &out);

  // Write any source rules.
  source_->WriteMakeFile(&out);

  // Write the make clean rule.
  Makefile::Rule* clean = out.StartRule("clean", "");
  for (const Node* node : process_order) {
    node->WriteMakeClean(clean);
  }
  source_->WriteMakeClean(clean);
  clean->WriteCommand("rm -rf " + input.object_dir());
  clean->WriteCommand("rm -rf " + input.binary_dir());
  clean->WriteCommand("rm -rf " + input.genfile_dir());
  clean->WriteCommand("rm -rf " + input.source_dir());
  clean->WriteCommand("rm -rf " + input.pkgfile_dir());
  out.FinishRule(clean);

  // Write the install rules.
  const char kInstallBoilerplate[] =
      "# http://www.gnu.org/prep/standards/standards.html\n"
      "prefix=/usr/local\n"
      "exec_prefix=$(prefix)\n"
      "bindir=$(exec_prefix)/bin\n"
      "includedir=$(prefix)/include\n"
      "libdir=$(exec_prefix)/lib\n"
      "INSTALL=install\n"
      "INSTALL_PROGRAM=$(INSTALL)\n"
      "INSTALL_DATA=$(INSTALL) -m 644\n\n";
  out.append(kInstallBoilerplate);
  Makefile::Rule* install = out.StartRule("install", "");
  for (const Node* node : parser.input_nodes()) {
    node->WriteMakeInstall(&out, install);
  }
  out.FinishRule(install);

  // Write the all rule.
  ResourceFileSet outputs;
  for (const Node* node : parser.input_nodes()) {
    if (node->IncludeInAll()) {
      node->FinalOutputs(Node::NO_LANG, &outputs);
      outputs.Add(Resource::FromRootPath(node->target().make_path()));
    }
  }
  out.WriteRule("all", strings::JoinAll(outputs.files(), " "));

  // Write the test rule.
  set<string> tests;
  for (const Node* node : parser.input_nodes()) {
    if (node->IncludeInTests()) {
      node->FinalTests(Node::NO_LANG, &tests);
    }
  }
  out.WriteRule("tests", strings::JoinAll(tests, " "));

  // Write the licences rule.
  Makefile::Rule* license_rule = out.StartRule("licenses");
  license_rule->WriteCommand("echo \"License information.\"");
  for (const Node* node : parser.input_nodes()) {
    set<string> licenses;
    node->Licenses(&licenses);
    string output = "printf \"" + node->target().full_path() + " =>\\n";
    for (const string& license : licenses) {
      output += "    " + license + "\\n";
    }
    output += "\\n\"";
    license_rule->WriteCommand(output);
  }
  out.FinishRule(license_rule);

  // Not real files:
  out.append(".PHONY: clean all tests install licenses\n\n");

  // Default build everything.
  out.append(".DEFAULT_GOAL=all\n\n");

  // And finalize.
  out.FinishMakefile();

  return out.out();
}

}  // namespace repobuild
