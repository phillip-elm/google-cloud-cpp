// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include <google/protobuf/text_format.h>
#include <deque>
#include <list>
#include <sstream>

namespace {
/**
 * The key used for ReadRow(), ReadModifyWrite(), CheckAndMutate.
 *
 * Using the same key makes it possible for the user to see the effect of
 * the different APIs on one row.
 */

struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << std::endl;
}

//! [create table]
void CreateTable(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"create-table: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  auto schema = admin.CreateTable(
      table_id,
      google::cloud::bigtable::TableConfig(
          {{"fam", google::cloud::bigtable::GcRule::MaxNumVersions(10)},
           {"foo",
            google::cloud::bigtable::GcRule::MaxAge(std::chrono::hours(72))}},
          {}));
}
//! [create table]

//! [list tables]
void ListTables(google::cloud::bigtable::TableAdmin admin, int argc,
                char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-tables: <project-id> <instanse-id>"};
  }
  auto tables =
      admin.ListTables(google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED);
  for (auto const& table : tables) {
    std::cout << table.name() << std::endl;
  }
}
//! [list tables]

//! [get table]
void GetTable(google::cloud::bigtable::TableAdmin admin, int argc,
              char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-table: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  auto table =
      admin.GetTable(table_id, google::bigtable::admin::v2::Table::FULL);
  std::cout << table.name() << "\n";
  for (auto const& family : table.column_families()) {
    std::string const& family_name = family.first;
    std::string gc_rule;
    google::protobuf::TextFormat::PrintToString(family.second.gc_rule(),
                                                &gc_rule);
    std::cout << "\t" << family_name << "\t\t" << gc_rule << std::endl;
  }
}
//! [get table]

//! [delete table]
void DeleteTable(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-table: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  admin.DeleteTable(table_id);
}
//! [delete table]

//! [modify table]
void ModifyTable(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"modify-table: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  auto schema = admin.ModifyColumnFamilies(
      table_id,
      {google::cloud::bigtable::ColumnFamilyModification::Drop("foo"),
       google::cloud::bigtable::ColumnFamilyModification::Update(
           "fam", google::cloud::bigtable::GcRule::Union(
                      google::cloud::bigtable::GcRule::MaxNumVersions(5),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(24 * 7)))),
       google::cloud::bigtable::ColumnFamilyModification::Create(
           "bar", google::cloud::bigtable::GcRule::Intersection(
                      google::cloud::bigtable::GcRule::MaxNumVersions(3),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(72))))});

  std::string formatted;
  google::protobuf::TextFormat::PrintToString(schema, &formatted);
  std::cout << "Schema modified to: " << formatted << std::endl;
}
//! [modify table]

//! [drop all rows]
void DropAllRows(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"drop-all-rows: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  admin.DropAllRows(table_id);
}
//! [drop all rows]

//! [drop rows by prefix]
void DropRowsByPrefix(google::cloud::bigtable::TableAdmin admin, int argc,
                      char* argv[]) {
  if (argc != 2) {
    throw Usage{"drop-rows-by-prefix: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  admin.DropRowsByPrefix(table_id, "key-00004");
}
//! [drop rows by prefix]

//! [wait for consistency check]
void WaitForConsistencyCheck(google::cloud::bigtable::TableAdmin admin,
                             int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{
        "wait-for-consistency-check: <project-id> <instanse-id> <table-id>"};
  }
  std::string const table_id_param = ConsumeArg(argc, argv);

  google::cloud::bigtable::TableId table_id(table_id_param);
  google::cloud::bigtable::ConsistencyToken consistency_token(
      admin.GenerateConsistencyToken(table_id.get()));
  auto result = admin.WaitForConsistencyCheck(table_id, consistency_token);
  if (result.get()) {
    std::cout << "Table is consistent" << std::endl;
  } else {
    std::cout << "Table is not consistent" << std::endl;
  }
}
//! [wait for consistency check]

//! [check consistency]
void CheckConsistency(google::cloud::bigtable::TableAdmin admin, int argc,
                      char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "check-consistency: <project-id> <instanse-id> <table-id> "
        "<consistency_token"};
  }
  std::string const table_id_param = ConsumeArg(argc, argv);
  std::string const consistency_token_param = ConsumeArg(argc, argv);

  google::cloud::bigtable::TableId table_id(table_id_param);
  google::cloud::bigtable::ConsistencyToken consistency_token(
      consistency_token_param);
  auto result = admin.CheckConsistency(table_id, consistency_token);
  if (result) {
    std::cout << "Table is consistent" << std::endl;
  } else {
    std::cout << "Table is not yet consistent, Please Try again Later with the "
                 "same Token!";
  }
  std::cout << std::flush;
}
//! [check consistency]

//! [get snapshot]
void GetSnapshot(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "get-snapshot: <project-id> <instanse-id> <cluster-id> <snapshot-id"};
  }
  std::string const cluster_id_str = ConsumeArg(argc, argv);
  std::string const snapshot_id_str = ConsumeArg(argc, argv);

  google::cloud::bigtable::ClusterId cluster_id(cluster_id_str);
  google::cloud::bigtable::SnapshotId snapshot_id(snapshot_id_str);
  auto snapshot = admin.GetSnapshot(cluster_id, snapshot_id);
  std::cout << "GetSnapshot name : " << snapshot.name() << std::endl;
}
//! [get snapshot]

//! [list snapshots]
void ListSnapshots(google::cloud::bigtable::TableAdmin admin, int argc,
                   char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-snapshot: <project-id> <instanse-id> <cluster-id>"};
  }
  std::string const cluster_id_str = ConsumeArg(argc, argv);

  google::cloud::bigtable::ClusterId cluster_id(cluster_id_str);

  auto snapshot_list = admin.ListSnapshots(cluster_id);
  std::cout << "Snapshot Name List" << std::endl;
  for (auto const& snapshot : snapshot_list) {
    std::cout << "Snapshot Name:" << snapshot.name() << std::endl;
  }
}
//! [list snapshots]

//! [delete snapshot]
void DeleteSnapshot(google::cloud::bigtable::TableAdmin admin, int argc,
                    char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "delete-snapshot: <project-id> <instanse-id> <cluster-id> "
        "<snapshot-id"};
  }
  std::string const cluster_id_str = ConsumeArg(argc, argv);
  std::string const snapshot_id_str = ConsumeArg(argc, argv);

  google::cloud::bigtable::ClusterId cluster_id(cluster_id_str);
  google::cloud::bigtable::SnapshotId snapshot_id(snapshot_id_str);
  admin.DeleteSnapshot(cluster_id, snapshot_id);
}
//! [delete snapshot]

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType =
      std::function<void(google::cloud::bigtable::TableAdmin, int, char* [])>;

  std::map<std::string, CommandType> commands = {
      {"create-table", &CreateTable},
      {"list-tables", &ListTables},
      {"get-table", &GetTable},
      {"delete-table", &DeleteTable},
      {"modify-table", &ModifyTable},
      {"drop-all-rows", &DropAllRows},
      {"drop-rows-by-prefix", &DropRowsByPrefix},
      {"wait-for-consistency-check", &WaitForConsistencyCheck},
      {"check-consistency", &CheckConsistency},
      {"get-snapshot", &GetSnapshot},
      {"list-snapshot", &ListSnapshots},
      {"delete-snapshot", &DeleteSnapshot},
  };

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an TableAdmin
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    google::cloud::bigtable::TableAdmin unused(
        google::cloud::bigtable::CreateDefaultAdminClient(
            "unused-project", google::cloud::bigtable::ClientOptions()),
        "Unused-instance");
    for (auto&& kv : commands) {
      try {
        int fake_argc = 0;
        kv.second(unused, fake_argc, argv);
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      } catch (...) {
        // ignore other exceptions.
      }
    }
  }

  if (argc < 4) {
    PrintUsage(argc, argv, "Missing command and/or project-id/ or instance-id");
    return 1;
  }

  std::string const command_name = ConsumeArg(argc, argv);
  std::string const project_id = ConsumeArg(argc, argv);
  std::string const instance_id = ConsumeArg(argc, argv);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argc, argv, "Unknown command: " + command_name);
    return 1;
  }

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect admin]
  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);
  //! [connect admin]

  command->second(admin, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
