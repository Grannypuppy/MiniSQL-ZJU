#include "executor/execute_engine.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>

#include "common/result_writer.h"
#include "executor/executors/delete_executor.h"
#include "executor/executors/index_scan_executor.h"
#include "executor/executors/insert_executor.h"
#include "executor/executors/seq_scan_executor.h"
#include "executor/executors/update_executor.h"
#include "executor/executors/values_executor.h"
#include "glog/logging.h"
#include "planner/planner.h"
#include "utils/utils.h"
extern "C" {
int yyparse(void);
//FILE *yyin;
#include "parser/minisql_lex.h"
#include "parser/parser.h"
}

ExecuteEngine::ExecuteEngine() {
  char path[] = "./databases";
  DIR *dir;
  if ((dir = opendir(path)) == nullptr) {
    mkdir("./databases", 0777);
    dir = opendir(path);
  }
  struct dirent *stdir;
  while((stdir = readdir(dir)) != nullptr) {
    if( strcmp( stdir->d_name , "." ) == 0 ||
        strcmp( stdir->d_name , "..") == 0 ||
        stdir->d_name[0] == '.')
      continue;
    dbs_[stdir->d_name] = new DBStorageEngine(stdir->d_name, false);
  }
  
  closedir(dir);
}

std::unique_ptr<AbstractExecutor> ExecuteEngine::CreateExecutor(ExecuteContext *exec_ctx,
                                                                const AbstractPlanNodeRef &plan) {
  switch (plan->GetType()) {
    // Create a new sequential scan executor
    case PlanType::SeqScan: {
      return std::make_unique<SeqScanExecutor>(exec_ctx, dynamic_cast<const SeqScanPlanNode *>(plan.get()));
    }
    // Create a new index scan executor
    case PlanType::IndexScan: {
      return std::make_unique<IndexScanExecutor>(exec_ctx, dynamic_cast<const IndexScanPlanNode *>(plan.get()));
    }
    // Create a new update executor
    case PlanType::Update: {
      auto update_plan = dynamic_cast<const UpdatePlanNode *>(plan.get());
      auto child_executor = CreateExecutor(exec_ctx, update_plan->GetChildPlan());
      return std::make_unique<UpdateExecutor>(exec_ctx, update_plan, std::move(child_executor));
    }
      // Create a new delete executor
    case PlanType::Delete: {
      auto delete_plan = dynamic_cast<const DeletePlanNode *>(plan.get());
      auto child_executor = CreateExecutor(exec_ctx, delete_plan->GetChildPlan());
      return std::make_unique<DeleteExecutor>(exec_ctx, delete_plan, std::move(child_executor));
    }
    case PlanType::Insert: {
      auto insert_plan = dynamic_cast<const InsertPlanNode *>(plan.get());
      auto child_executor = CreateExecutor(exec_ctx, insert_plan->GetChildPlan());
      return std::make_unique<InsertExecutor>(exec_ctx, insert_plan, std::move(child_executor));
    }
    case PlanType::Values: {
      return std::make_unique<ValuesExecutor>(exec_ctx, dynamic_cast<const ValuesPlanNode *>(plan.get()));
    }
    default:
      throw std::logic_error("Unsupported plan type.");
  }
}

dberr_t ExecuteEngine::ExecutePlan(const AbstractPlanNodeRef &plan, std::vector<Row> *result_set, Txn *txn,
                                   ExecuteContext *exec_ctx) {
  // Construct the executor for the abstract plan node
  auto executor = CreateExecutor(exec_ctx, plan);

  try {
    executor->Init();
    RowId rid{};
    Row row{};
    while (executor->Next(&row, &rid)) {
      if (result_set != nullptr) {
        result_set->push_back(row);
      }
    }
  } catch (const exception &ex) {
    std::cout << "Error Encountered in Executor Execution: " << ex.what() << std::endl;
    if (result_set != nullptr) {
      result_set->clear();
    }
    return DB_FAILED;
  }
  return DB_SUCCESS;
}

dberr_t ExecuteEngine::Execute(pSyntaxNode ast) {
  if (ast == nullptr) {
    return DB_FAILED;
  }
  auto start_time = std::chrono::system_clock::now();
  unique_ptr<ExecuteContext> context(nullptr);
  if (!current_db_.empty()) context = dbs_[current_db_]->MakeExecuteContext(nullptr);
  switch (ast->type_) {
    case kNodeCreateDB:
      return ExecuteCreateDatabase(ast, context.get());
    case kNodeDropDB:
      return ExecuteDropDatabase(ast, context.get());
    case kNodeShowDB:
      return ExecuteShowDatabases(ast, context.get());
    case kNodeUseDB:
      return ExecuteUseDatabase(ast, context.get());
    case kNodeShowTables:
      return ExecuteShowTables(ast, context.get());
    case kNodeCreateTable:
      return ExecuteCreateTable(ast, context.get());
    case kNodeDropTable:
      return ExecuteDropTable(ast, context.get());
    case kNodeShowIndexes:
      return ExecuteShowIndexes(ast, context.get());
    case kNodeCreateIndex:
      return ExecuteCreateIndex(ast, context.get());
    case kNodeDropIndex:
      return ExecuteDropIndex(ast, context.get());
    case kNodeTrxBegin:
      return ExecuteTrxBegin(ast, context.get());
    case kNodeTrxCommit:
      return ExecuteTrxCommit(ast, context.get());
    case kNodeTrxRollback:
      return ExecuteTrxRollback(ast, context.get());
    case kNodeExecFile:
      return ExecuteExecfile(ast, context.get());
    case kNodeQuit:
      return ExecuteQuit(ast, context.get());
    default:
      break;
  }
  // Plan the query.
  Planner planner(context.get());
  std::vector<Row> result_set{};
  try {
    planner.PlanQuery(ast);
    // Execute the query.
    ExecutePlan(planner.plan_, &result_set, nullptr, context.get());
  } catch (const exception &ex) {
    std::cout << "Error Encountered in Planner: " << ex.what() << std::endl;
    return DB_FAILED;
  }
  auto stop_time = std::chrono::system_clock::now();
  double duration_time =
      double((std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time)).count());
  // Return the result set as string.
  std::stringstream ss;
  ResultWriter writer(ss);

  if (planner.plan_->GetType() == PlanType::SeqScan || planner.plan_->GetType() == PlanType::IndexScan) {
    auto schema = planner.plan_->OutputSchema();
    auto num_of_columns = schema->GetColumnCount();
    if (!result_set.empty()) {
      // find the max width for each column
      vector<int> data_width(num_of_columns, 0);
      for (const auto &row : result_set) {
        for (uint32_t i = 0; i < num_of_columns; i++) {
          data_width[i] = max(data_width[i], int(row.GetField(i)->toString().size()));
        }
      }
      int k = 0;
      for (const auto &column : schema->GetColumns()) {
        data_width[k] = max(data_width[k], int(column->GetName().length()));
        k++;
      }
      // Generate header for the result set.
      writer.Divider(data_width);
      k = 0;
      writer.BeginRow();
      for (const auto &column : schema->GetColumns()) {
        writer.WriteHeaderCell(column->GetName(), data_width[k++]);
      }
      writer.EndRow();
      writer.Divider(data_width);

      // Transforming result set into strings.
      for (const auto &row : result_set) {
        writer.BeginRow();
        for (uint32_t i = 0; i < schema->GetColumnCount(); i++) {
          writer.WriteCell(row.GetField(i)->toString(), data_width[i]);
        }
        writer.EndRow();
      }
      writer.Divider(data_width);
    }
    writer.EndInformation(result_set.size(), duration_time, true);
  } else {
    writer.EndInformation(result_set.size(), duration_time, false);
  }
  std::cout << writer.stream_.rdbuf();
  // todo:: use shared_ptr for schema
  if (ast->type_ == kNodeSelect)
      delete planner.plan_->OutputSchema();
  return DB_SUCCESS;
}

void ExecuteEngine::ExecuteInformation(dberr_t result) {
  switch (result) {
    case DB_ALREADY_EXIST:
      cout << "Database already exists." << endl;
      break;
    case DB_NOT_EXIST:
      cout << "Database not exists." << endl;
      break;
    case DB_TABLE_ALREADY_EXIST:
      cout << "Table already exists." << endl;
      break;
    case DB_TABLE_NOT_EXIST:
      cout << "Table not exists." << endl;
      break;
    case DB_INDEX_ALREADY_EXIST:
      cout << "Index already exists." << endl;
      break;
    case DB_INDEX_NOT_FOUND:
      cout << "Index not exists." << endl;
      break;
    case DB_COLUMN_NAME_NOT_EXIST:
      cout << "Column not exists." << endl;
      break;
    case DB_KEY_NOT_FOUND:
      cout << "Key not exists." << endl;
      break;
    case DB_QUIT:
      cout << "Bye." << endl;
      break;
    default:
      break;
  }
}

dberr_t ExecuteEngine::ExecuteCreateDatabase(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateDatabase" << std::endl;
#endif
  string db_name = ast->child_->val_;
  if (dbs_.find(db_name) != dbs_.end()) {
    return DB_ALREADY_EXIST;
  }
  dbs_.insert(make_pair(db_name, new DBStorageEngine(db_name, true)));
  return DB_SUCCESS;
}

dberr_t ExecuteEngine::ExecuteDropDatabase(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteDropDatabase" << std::endl;
#endif
  string db_name = ast->child_->val_;
  if (dbs_.find(db_name) == dbs_.end()) {
    return DB_NOT_EXIST;
  }
  remove(("./databases/" + db_name).c_str());
  delete dbs_[db_name];
  dbs_.erase(db_name);
  if (db_name == current_db_)
    current_db_ = "";
  return DB_SUCCESS;
}

dberr_t ExecuteEngine::ExecuteShowDatabases(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteShowDatabases" << std::endl;
#endif
  if (dbs_.empty()) {
    cout << "Empty set (0.00 sec)" << endl;
    return DB_SUCCESS;
  }
  int max_width = 8;
  for (const auto &itr : dbs_) {
    if (itr.first.length() > max_width) max_width = itr.first.length();
  }
  cout << "+" << setfill('-') << setw(max_width + 2) << ""
       << "+" << endl;
  cout << "| " << std::left << setfill(' ') << setw(max_width) << "Database"
       << " |" << endl;
  cout << "+" << setfill('-') << setw(max_width + 2) << ""
       << "+" << endl;
  for (const auto &itr : dbs_) {
    cout << "| " << std::left << setfill(' ') << setw(max_width) << itr.first << " |" << endl;
  }
  cout << "+" << setfill('-') << setw(max_width + 2) << ""
       << "+" << endl;
  return DB_SUCCESS;
}

dberr_t ExecuteEngine::ExecuteUseDatabase(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteUseDatabase" << std::endl;
#endif
  string db_name = ast->child_->val_;
  if (dbs_.find(db_name) != dbs_.end()) {
    current_db_ = db_name;
    cout << "Database changed" << endl;
    return DB_SUCCESS;
  }
  return DB_NOT_EXIST;
}

dberr_t ExecuteEngine::ExecuteShowTables(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteShowTables" << std::endl;
#endif
  if (current_db_.empty()) {
    cout << "No database selected" << endl;
    return DB_FAILED;
  }
  vector<TableInfo *> tables;
  if (dbs_[current_db_]->catalog_mgr_->GetTables(tables) == DB_FAILED) {
    cout << "Empty set (0.00 sec)" << endl;
    return DB_FAILED;
  }
  string table_in_db("Tables_in_" + current_db_);
  uint max_width = table_in_db.length();
  for (const auto &itr : tables) {
    if (itr->GetTableName().length() > max_width) max_width = itr->GetTableName().length();
  }
  cout << "+" << setfill('-') << setw(max_width + 2) << ""
       << "+" << endl;
  cout << "| " << std::left << setfill(' ') << setw(max_width) << table_in_db << " |" << endl;
  cout << "+" << setfill('-') << setw(max_width + 2) << ""
       << "+" << endl;
  for (const auto &itr : tables) {
    cout << "| " << std::left << setfill(' ') << setw(max_width) << itr->GetTableName() << " |" << endl;
  }
  cout << "+" << setfill('-') << setw(max_width + 2) << ""
       << "+" << endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
struct ParsedColumnInfo {
  std::string column_name;
  TypeId type_id{TypeId::kTypeInvalid};
  uint32_t len_for_char{0};
  bool is_not_null{false};
  bool is_unique{false};
};
dberr_t ExecuteEngine::ExecuteCreateTable(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateTable" << std::endl;
#endif

  if (context == nullptr || current_db_.empty()) {
    std::cout << "context null Or No database selected." << std::endl;
    return DB_FAILED;
  }
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }
  Txn *txn = context->GetTransaction();

  //获取表
  if (ast == nullptr || ast->type_ != kNodeCreateTable || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for CREATE TABLE statement (missing table name).";
    return DB_FAILED;
  }
  std::string table_name(ast->child_->val_);
  if (table_name.empty()) {
    LOG(ERROR) << "Syntax error: Table name for CREATE TABLE cannot be empty.";
    return DB_FAILED;
  }
  //LOG(WARNING)<<"flag1"<<std::endl;
  // 初始化变量解析
  std::vector<ParsedColumnInfo> parsed_col_definitions;
  std::vector<std::string> parsed_column_list_from_ast;
  std::set<std::string> parsed_column_set_for_lookup;

  pSyntaxNode col_def_list_node = ast->child_->next_;
  if (col_def_list_node == nullptr || col_def_list_node->type_ != kNodeColumnDefinitionList) {
    LOG(ERROR) << "Syntax error: Invalid column definition list in CREATE TABLE statement.";
    return DB_FAILED;
  }

  pSyntaxNode current_item_node = col_def_list_node->child_;
  

  while (current_item_node != nullptr) {
    //LOG(WARNING)<<"flag2"<<std::endl;
    
    if (current_item_node->type_ == kNodeColumnDefinition) {
      ParsedColumnInfo parsed_col_info;
      pSyntaxNode col_name_node = current_item_node->child_;
      if (col_name_node == nullptr || col_name_node->type_ != kNodeIdentifier || col_name_node->val_ == nullptr) {
        LOG(ERROR) << "Syntax error: Column name missing in column definition.";
        return DB_FAILED;
      }
    
    parsed_col_info.column_name = col_name_node->val_;

    pSyntaxNode col_type_node = col_name_node->next_;
    if (col_type_node == nullptr || col_type_node->type_ != kNodeColumnType) {
      LOG(ERROR) << "Syntax error: Column type missing in column definition for column '" << parsed_col_info.column_name << "'.";
      return DB_FAILED;
    }

    std::string col_type_str(col_type_node->val_);
    std::transform(col_type_str.begin(), col_type_str.end(), col_type_str.begin(), ::tolower);
    //LOG(WARNING)<<"flag3 & col_type_str: "<<col_type_str<<std::endl;
    if (col_type_str == "int") {
      parsed_col_info.type_id = TypeId::kTypeInt;
      //LOG(WARNING)<<"flag_int"<<std::endl;
    } else if (col_type_str == "float") {
      parsed_col_info.type_id = TypeId::kTypeFloat;
      //LOG(WARNING)<<"flag_float"<<std::endl;
    } else if (col_type_str == "char") {
      //LOG(WARNING)<<"col_type_str is char, parsing length";
      parsed_col_info.type_id = TypeId::kTypeChar;
      pSyntaxNode col_length_node = col_type_node->child_;
      if (col_length_node == nullptr || col_length_node->type_ != kNodeNumber) {
        LOG(ERROR) << "Syntax error: CHAR Length missing for CHAR type in column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      std::string char_len_str(col_length_node->val_);
      //LOG(WARNING)<<"flag_char_len_str: "<<char_len_str<<std::endl;
      if(char_len_str.empty()) {
        LOG(ERROR) << "Syntax error: CHAR Length cannot be empty for column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      if (char_len_str.find('.')!=std::string::npos) {
        LOG(ERROR) << "Syntax error: CHAR Length cannot be a float for column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      if (char_len_str.find('-')!=std::string::npos) {
        LOG(ERROR) << "Syntax error: CHAR Length cannot be negative for column '" << parsed_col_info.column_name << "'.";
        return DB_FAILED;
      }
      try {
        unsigned long char_len = std::stoul(char_len_str);
        if (char_len > std::numeric_limits<uint32_t>::max()) {
          LOG(ERROR) << "Syntax error: CHAR Length exceeds maximum allowed length for column '" << parsed_col_info.column_name << "'.";
          return DB_FAILED;
        }
        parsed_col_info.len_for_char = static_cast<uint32_t>(char_len);
      } catch (const std::invalid_argument &ia) {
          LOG(ERROR) << "Syntax error: Invalid character in length specification for CHAR column '" << parsed_col_info.column_name << "' ('" << char_len_str << "'). Length must be a positive integer.";
          return DB_FAILED;
        } catch (const std::out_of_range &oor) {
          LOG(ERROR) << "Syntax error: Length for CHAR column '" << parsed_col_info.column_name << "' is out of range for unsigned long ('" << char_len_str << "').";
          return DB_FAILED;
        }
        if (parsed_col_info.len_for_char == 0) {
          LOG(ERROR) << "Invalid length " << parsed_col_info.len_for_char << " for CHAR column '" << parsed_col_info.column_name<< "'. Must be a positive integer greater than 0";
          return DB_FAILED;
        }
    } else {
      LOG(ERROR) << "Syntax error: Unsupported column type '" << col_type_str << "' in column definition.";
      return DB_FAILED;
    }

    if (current_item_node->val_ != nullptr) {
      std::string constraints(current_item_node->val_);
      std::transform(constraints.begin(), constraints.end(), constraints.begin(), ::tolower);
      if (constraints.find("not null") != std::string::npos) {
        parsed_col_info.is_not_null = true;
      }
      if (constraints.find("unique") != std::string::npos) {
        parsed_col_info.is_unique = true;
      }
    }
    parsed_col_definitions.push_back(parsed_col_info);

    }else if (current_item_node->type_ == kNodeColumnList) {
      // 处理列列表
      pSyntaxNode col_list_node = current_item_node->child_;
      while (col_list_node != nullptr) {
        if (col_list_node->type_ == kNodeIdentifier && col_list_node->val_ != nullptr) {
          ParsedColumnInfo parsed_col_info;
          parsed_col_info.column_name = col_list_node->val_;
          parsed_column_list_from_ast.push_back(parsed_col_info.column_name);
          parsed_column_set_for_lookup.insert(parsed_col_info.column_name);
          col_list_node = col_list_node->next_;
        } else {
          LOG(ERROR) << "Syntax error: Expected column name in PRIMARY KEY constraint for table '" << table_name << "'.";
          return DB_FAILED;
        }
        col_list_node = col_list_node->next_;
      }
    } else {
      LOG(ERROR) << "Syntax error: Invalid node type in column definition list.";
      return DB_FAILED;
    }  
    current_item_node = current_item_node->next_;
  }

  if (parsed_col_definitions.empty() && parsed_column_list_from_ast.empty()) {
    LOG(ERROR) << "Syntax error: No columns defined in CREATE TABLE statement for table '" << table_name << "'.";
    return DB_FAILED;
  }

  // 验证主键的列名是否都在已经定义的列中
  for (const auto &col_name : parsed_column_list_from_ast) {
    bool check = false;
    for (const auto &parsed_col_def : parsed_col_definitions) {
      if (parsed_col_def.column_name == col_name) {
        // 如果列名在定义中，跳过检查
        check = true;
        break;
      }
    }
    if (!check) {
      LOG(ERROR) << "Syntax error: Column '" << col_name << "' in PRIMARY KEY constraint not defined in table '" << table_name << "'.";
      ExecuteInformation(DB_COLUMN_NAME_NOT_EXIST); 
      return DB_COLUMN_NAME_NOT_EXIST;
    }
  }

  // 开始创建Column对象
  std::vector<Column*> columns;
  uint32_t index = 0;
  for (const auto &parsed_col_def : parsed_col_definitions) {
    bool is_primary_key = (parsed_column_set_for_lookup.find(parsed_col_def.column_name) != parsed_column_set_for_lookup.end());
    bool is_unique = parsed_col_def.is_unique || is_primary_key;
    bool is_nullable = !parsed_col_def.is_not_null && !is_primary_key; // 主键列不能为NULL
    
    Column *column = nullptr;
    if (parsed_col_def.type_id == TypeId::kTypeChar) {
      column = new Column(parsed_col_def.column_name, parsed_col_def.type_id, parsed_col_def.len_for_char, index, is_nullable, is_unique);
    } else {
      column = new Column(parsed_col_def.column_name, parsed_col_def.type_id, index, is_nullable, is_unique);
    }
    columns.push_back(column);
    index++;
  }

  // 根据columns创建Schema
  TableSchema *schema = new TableSchema(columns, true);

  // 调用CatalogManager的CreateTable方法
  TableInfo *table_info = nullptr;
  dberr_t create_result = catalog_manager->CreateTable(table_name, schema, txn, table_info);

  delete schema; // 释放Schema内存(如果CreateTable成功，里面是深拷贝会开新的Schema对象)

  if (create_result != DB_SUCCESS) {
    LOG(ERROR) << "Failed to create table '" << table_name << "' in database '" << current_db_ << "'. Error code: " << create_result;
    ExecuteInformation(create_result);
    return create_result;
  }

  // 创建primary_key的索引（如果有primary_key）
  if(!parsed_column_list_from_ast.empty()) {
    std::string index_name = table_name + "_primary_key";
    IndexInfo *index_info = nullptr;
    dberr_t index_create_result = catalog_manager->CreateIndex(table_name, index_name, parsed_column_list_from_ast, txn, index_info,"bptree");
    if (index_create_result != DB_SUCCESS) {
      LOG(ERROR) << "Failed to create primary key index '" << index_name << "' for table '" << table_name << "'. Error code: " << index_create_result<<endl;
      // 尝试回滚，删除刚才创建的table
      dberr_t drop_table_result = catalog_manager->DropTable(table_name);
      if (drop_table_result != DB_SUCCESS) {
        LOG(ERROR) << "Failed to rollback table '" << table_name << "' after index creation failure."<<endl;
      }
      else {
        LOG(INFO) << "Have Rolled back table '" << table_name << "' after Primary_Key index creation failure."<<endl;
      }
      ExecuteInformation(index_create_result);
      return index_create_result;
    }
  }

  // 根据其他unique键，创建索引
  for (const auto& parsed_col_def : parsed_col_definitions) {
    if (parsed_col_def.is_unique && parsed_column_set_for_lookup.find(parsed_col_def.column_name) == parsed_column_set_for_lookup.end()) {
      std::string index_name = table_name + "_" + parsed_col_def.column_name + "_unique";
      IndexInfo *index_info = nullptr;
      dberr_t index_create_result = catalog_manager->CreateIndex(table_name, index_name, {parsed_col_def.column_name}, txn, index_info,"bptree");
      if (index_create_result != DB_SUCCESS) {
        LOG(WARNING) << "Failed to create unique index '" << index_name << "' for column '" << parsed_col_def.column_name << "' in table '" << table_name <<"'"<<endl;
      }
    }
  }

  LOG(INFO) << "Table '" << table_name << "' created successfully."<< std::endl;

  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteDropTable(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteDropTable" << std::endl;
#endif
  // 检查是否有选中的数据库
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }
  // 获取要删除的表名
  if (ast == nullptr || ast->type_ != kNodeDropTable || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for DROP TABLE statement (missing table name).";
    return DB_FAILED;
  }
  std::string table_name(ast->child_->val_);
  if (table_name.empty()) {
      LOG(ERROR) << "Syntax error: Table name for DROP TABLE cannot be empty.";
      return DB_FAILED;
  }
  // 尝试在 CatalogManager 中删除表
  dberr_t res = catalog_manager->DropTable(table_name);

  // 如果删除失败，返回错误码
  if (res != DB_SUCCESS) {
    ExecuteInformation(res);
    return res;
  }

  std::cout << "Table [" << table_name << "] dropped successfully." << std::endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteShowIndexes(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteShowIndexes" << std::endl;
#endif
  
  // 验证执行上下文和数据库选择状态
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }

  // 获取当前数据库中的所有表
  std::vector<TableInfo *> all_tables;
  dberr_t get_tables_result = catalog_manager->GetTables(all_tables);

  if (get_tables_result != DB_SUCCESS) {
    if (get_tables_result == DB_TABLE_NOT_EXIST) {
      std::cout << "No index exists in database '" << current_db_ << "' (no tables found)." << std::endl;
      return DB_SUCCESS;
    }
    ExecuteInformation(get_tables_result);
    return get_tables_result;
  }

  // 用于跟踪是否找到任何索引和控制输出格式
  bool found_any_index = false;
  std::stringstream output_stream;
  ResultWriter result_writer(output_stream);
  bool is_first_table_output = true;

  // 遍历数据库中的每个表
  for (TableInfo *table_info_ptr : all_tables) {
    if (table_info_ptr == nullptr) {
      LOG(WARNING) << "Encountered null TableInfo pointer during table iteration in ExecuteShowIndexes.";
      continue;
    }

    std::string table_name = table_info_ptr->GetTableName();
    std::vector<IndexInfo *> table_indexes;

    // 获取当前表的所有索引
    dberr_t get_indexes_result = catalog_manager->GetTableIndexes(table_name, table_indexes);

    if (get_indexes_result != DB_SUCCESS && get_indexes_result != DB_INDEX_NOT_FOUND) {
      ExecuteInformation(get_indexes_result);
      return get_indexes_result;
    }

    // 如果当前表没有索引，跳过
    if (table_indexes.empty()) {
      continue;
    }

    found_any_index = true;

    // 在多个表的索引列表之间添加空行分隔
    if (!is_first_table_output) {
      output_stream << std::endl;
    }
    is_first_table_output = false;

    // 计算当前表索引显示所需的列宽
    std::string table_header = "Indexes_in_" + table_name;
    int column_width = static_cast<int>(table_header.length());
    
    for (IndexInfo *index_info_ptr : table_indexes) {
      if (index_info_ptr != nullptr) {
        column_width = std::max(column_width, static_cast<int>(index_info_ptr->GetIndexName().length()));
      }
    }
    
    // 确保列宽足够显示基本标题
    column_width = std::max(column_width, static_cast<int>(std::string("Index").length()));
    std::vector<int> column_widths = {column_width};

    // 输出当前表的索引列表表头
    result_writer.Divider(column_widths);
    result_writer.BeginRow();
    result_writer.WriteHeaderCell(table_header, column_width);
    result_writer.EndRow();
    result_writer.Divider(column_widths);

    // 输出当前表的每个索引名称
    for (IndexInfo *index_info_ptr : table_indexes) {
      if (index_info_ptr != nullptr) {
        result_writer.BeginRow();
        result_writer.WriteCell(index_info_ptr->GetIndexName(), column_width);
        result_writer.EndRow();
      }
    }
    result_writer.Divider(column_widths);
  }

  // 根据是否找到索引输出相应结果
  if (found_any_index) {
    std::cout << output_stream.str();
  } else {
    std::cout << "No index exists in database '" << current_db_ << "'." << std::endl;
  }
  
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteCreateIndex(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateIndex" << std::endl;
#endif
  
  // 验证执行上下文和数据库状态
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }
  Txn *txn = context->GetTransaction();

  // 解析 AST 获取索引名称
  if (ast == nullptr || ast->type_ != kNodeCreateIndex || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST for CREATE INDEX (missing index name).";
    return DB_FAILED;
  }
  std::string index_name(ast->child_->val_);

  // 解析表名
  pSyntaxNode table_name_node = ast->child_->next_;
  if (table_name_node == nullptr || table_name_node->type_ != kNodeIdentifier || table_name_node->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST for CREATE INDEX (missing table name for index '" << index_name << "').";
    return DB_FAILED;
  }
  std::string table_name(table_name_node->val_);

  // 解析列名列表
  pSyntaxNode column_list_node = table_name_node->next_;
  if (column_list_node == nullptr || column_list_node->type_ != kNodeColumnList || column_list_node->child_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST for CREATE INDEX (missing column list for index keys on index '" << index_name << "').";
    return DB_FAILED;
  }
  
  std::vector<std::string> index_column_names;
  pSyntaxNode current_column_node = column_list_node->child_;
  while (current_column_node != nullptr) {
    if (current_column_node->type_ != kNodeIdentifier || current_column_node->val_ == nullptr) {
      LOG(ERROR) << "Syntax error: Expected column name in index key list for index '" << index_name << "'.";
      return DB_FAILED;
    }
    index_column_names.push_back(std::string(current_column_node->val_));
    current_column_node = current_column_node->next_;
  }
  
  if (index_column_names.empty()) {
    LOG(ERROR) << "Syntax error: No columns specified for index '" << index_name << "'.";
    return DB_FAILED;
  }

  // 解析索引类型（如果有）（其实基本没有区别都是bptree）
  std::string index_type = "bptree";
  pSyntaxNode index_type_node = column_list_node->next_;
  if (index_type_node != nullptr && index_type_node->type_ == kNodeIndexType) {
    if (index_type_node->child_ != nullptr && index_type_node->child_->type_ == kNodeIdentifier &&
        index_type_node->child_->val_ != nullptr) {
      index_type = index_type_node->child_->val_;
    }
  }

  // 验证表的存在性并获取表信息
  TableInfo *table_info = nullptr;
  dberr_t get_table_result = catalog_manager->GetTable(table_name, table_info);
  if (get_table_result != DB_SUCCESS) {
    ExecuteInformation(get_table_result);
    return get_table_result;
  }
  ASSERT(table_info != nullptr, "GetTable succeeded but table_info is null.");

  // 验证列的存在性并构建列索引映射
  std::vector<uint32_t> column_index_mapping;
  column_index_mapping.reserve(index_column_names.size());
  TableSchema *table_schema = table_info->GetSchema();
  if (table_schema == nullptr) {
    LOG(ERROR) << "Table " << table_name << " has no schema. Cannot create index.";
    return DB_FAILED;
  }
  
  for (const std::string &column_name : index_column_names) {
    uint32_t column_index;
    if (table_schema->GetColumnIndex(column_name, column_index) != DB_SUCCESS) {
      LOG(ERROR) << "Column '" << column_name << "' not found in table '" << table_name 
                 << "' for index '" << index_name << "'.";
      ExecuteInformation(DB_COLUMN_NAME_NOT_EXIST);
      return DB_COLUMN_NAME_NOT_EXIST;
    }
    column_index_mapping.push_back(column_index);
  }

  // 在CatalogManager中创建索引
  IndexInfo *created_index_info = nullptr;
  dberr_t create_index_result = catalog_manager->CreateIndex(
      table_name, index_name, index_column_names, txn, created_index_info, index_type);

  if (create_index_result != DB_SUCCESS) {
    ExecuteInformation(create_index_result);
    return create_index_result;
  }
  ASSERT(created_index_info != nullptr, "CatalogManager::CreateIndex succeeded but output IndexInfo is null.");

  // 获取表堆用于遍历现有数据
  TableHeap *table_heap = table_info->GetTableHeap();
  if (table_heap == nullptr) {
    LOG(ERROR) << "Failed to get TableHeap for table '" << table_name << "' while populating index '" << index_name << "'.";
    catalog_manager->DropIndex(table_name, index_name);
    return DB_FAILED;
  }

  // 获取索引结构用于插入数据
  Index *index_structure = created_index_info->GetIndex();
  if (index_structure == nullptr) {
    LOG(ERROR) << "Newly created IndexInfo for index '" << index_name << "' does not have an initialized index structure.";
    catalog_manager->DropIndex(table_name, index_name);
    return DB_FAILED;
  }

  // 遍历表中现有记录并插入到索引中
  for (TableIterator table_iter = table_heap->Begin(txn); table_iter != table_heap->End(); ++table_iter) {
    Row current_row(table_iter->GetRowId());
    if (!table_heap->GetTuple(&current_row, txn)) {
      LOG(WARNING) << "Failed to get tuple for rowid (Page: " << table_iter->GetRowId().GetPageId() 
                   << ", Slot: " << table_iter->GetRowId().GetSlotNum() << ") during index population for " << index_name;
      continue; 
    }
    
    RowId row_id = current_row.GetRowId();

    // 构建索引键行
    std::vector<Field> index_key_fields;
    index_key_fields.reserve(column_index_mapping.size());
    for (uint32_t column_index : column_index_mapping) {
      index_key_fields.push_back(*(current_row.GetField(column_index))); 
    }
    Row index_key_row(index_key_fields);

    // 将记录插入索引
    if (index_structure->InsertEntry(index_key_row, row_id, txn) != DB_SUCCESS) {
      LOG(ERROR) << "Failed to insert entry into index '" << index_name << "' for rowid (Page: " 
                 << row_id.GetPageId() << ", Slot: " << row_id.GetSlotNum()
                 << ") during initial population.";
      catalog_manager->DropIndex(table_name, index_name);
      return DB_FAILED;
    }
  }
  
  std::cout << "Index [" << index_name << "] created successfully on table [" << table_name << "]." << std::endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteDropIndex(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteDropIndex" << std::endl;
#endif
  
  // 验证执行上下文和数据库状态
  if (context == nullptr || current_db_.empty()) {
    std::cout << "No database selected." << std::endl;
    return DB_FAILED;
  }
  
  CatalogManager *catalog_manager = context->GetCatalog();
  if (catalog_manager == nullptr) {
    LOG(ERROR) << "Critical error: CatalogManager is null in ExecuteContext for database " << current_db_;
    return DB_FAILED;
  }

  // 解析 AST 获取要删除的索引名称
  if (ast == nullptr || ast->type_ != kNodeDropIndex || ast->child_ == nullptr ||
      ast->child_->type_ != kNodeIdentifier || ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for DROP INDEX statement (missing index name).";
    return DB_FAILED;
  }
  
  std::string target_index_name(ast->child_->val_);
  if (target_index_name.empty()) {
    LOG(ERROR) << "Syntax error: Index name for DROP INDEX cannot be empty.";
    return DB_FAILED;
  }

  // 获取数据库中的所有表
  std::vector<TableInfo *> database_tables;
  dberr_t get_tables_result = catalog_manager->GetTables(database_tables);
  if (get_tables_result != DB_SUCCESS) {
    if (get_tables_result == DB_TABLE_NOT_EXIST) {
      ExecuteInformation(DB_INDEX_NOT_FOUND);
      return DB_INDEX_NOT_FOUND;
    }
    ExecuteInformation(get_tables_result);
    return get_tables_result;
  }

  // 遍历所有表查找目标索引
  for (TableInfo *table_info_ptr : database_tables) {
    if (table_info_ptr == nullptr) {
      continue;
    }

    std::string current_table_name = table_info_ptr->GetTableName();
    std::vector<IndexInfo *> table_index_list;

    // 获取当前表的所有索引
    dberr_t get_table_indexes_result = catalog_manager->GetTableIndexes(current_table_name, table_index_list);
    
    if (get_table_indexes_result == DB_SUCCESS) {
      // 在当前表的索引中查找目标索引
      for (IndexInfo *index_info_ptr : table_index_list) {
        if (index_info_ptr == nullptr) {
          continue;
        }

        if (index_info_ptr->GetIndexName() == target_index_name) {
          // 找到目标索引，执行删除操作
          dberr_t drop_index_result = catalog_manager->DropIndex(current_table_name, target_index_name);
          if (drop_index_result != DB_SUCCESS) {
            ExecuteInformation(drop_index_result);
            return drop_index_result;
          }
          std::cout << "Index [" << target_index_name << "] dropped successfully from table [" << current_table_name << "]." << std::endl;
          return DB_SUCCESS;
        }
      }
    } else if (get_table_indexes_result != DB_INDEX_NOT_FOUND) {
      LOG(ERROR) << "Error fetching indexes for table " << current_table_name << " during DROP INDEX operation.";
      ExecuteInformation(get_table_indexes_result);
      return get_table_indexes_result;
    }
  }
  
  // 如果遍历完所有表都没有找到目标索引
  ExecuteInformation(DB_INDEX_NOT_FOUND);
  return DB_INDEX_NOT_FOUND;
}

dberr_t ExecuteEngine::ExecuteTrxBegin(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteTrxBegin" << std::endl;
#endif
  return DB_FAILED;
}

dberr_t ExecuteEngine::ExecuteTrxCommit(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteTrxCommit" << std::endl;
#endif
  return DB_FAILED;
}

dberr_t ExecuteEngine::ExecuteTrxRollback(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteTrxRollback" << std::endl;
#endif
  return DB_FAILED;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteExecfile(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteExecfile" << std::endl;
#endif
  
  // 解析 AST 获取文件名
  if (ast == nullptr || ast->type_ != kNodeExecFile || ast->child_ == nullptr ||
      (ast->child_->type_ != kNodeString && ast->child_->type_ != kNodeIdentifier) ||
      ast->child_->val_ == nullptr) {
    LOG(ERROR) << "Syntax error: Invalid AST structure for EXECFILE statement (missing filename).";
    ExecuteInformation(DB_FAILED);
    return DB_FAILED;
  }
  
  std::string script_filename(ast->child_->val_);
  if (script_filename.empty()) {
    LOG(ERROR) << "Syntax error: Filename for EXECFILE cannot be empty.";
    ExecuteInformation(DB_FAILED);
    return DB_FAILED;
  }

  // 打开并验证 SQL 脚本文件
  std::ifstream script_file(script_filename);
  if (!script_file.is_open()) {
    LOG(ERROR) << "Cannot open file '" << script_filename << "' for EXECFILE.";
    std::cout << "Error: Cannot open SQL script file '" << script_filename << "'." << std::endl;
    return DB_FAILED;
  }

  std::cout << "Executing SQL script file [" << script_filename << "] ..." << std::endl;
  
  // 初始化执行状态变量
  std::string statement_buffer;
  char current_char;
  dberr_t execution_status = DB_SUCCESS;
  int current_line_number = 0;

  // 逐字符读取和处理文件内容
  while (script_file.get(current_char)) {
    statement_buffer += current_char;
    if (current_char == '\n') {
      current_line_number++;
    }

    if (current_char == ';') {
      // 清理语句字符串的首尾空白字符
      statement_buffer.erase(0, statement_buffer.find_first_not_of(" \t\n\r\f\v"));
      statement_buffer.erase(statement_buffer.find_last_not_of(" \t\n\r\f\v") + 1);

      if (statement_buffer.empty() || statement_buffer == ";") {
        statement_buffer.clear();
        continue;
      }

      // 初始化解析器并创建词法分析缓冲区
      MinisqlParserInit();
      YY_BUFFER_STATE lexer_buffer = yy_scan_string(statement_buffer.c_str());
      if (lexer_buffer == nullptr) {
        LOG(ERROR) << "Failed to create Flex buffer for SQL statement: " << statement_buffer;
        execution_status = DB_FAILED;
        MinisqlParserFinish();
        break;
      }
      
      // 执行语法分析
      int parsing_result = yyparse();
      yy_delete_buffer(lexer_buffer);
      pSyntaxNode statement_ast = MinisqlGetParserRootNode();

      if (parsing_result != 0 || statement_ast == nullptr || MinisqlParserGetError() != 0) {
        LOG(ERROR) << "Syntax error in file '" << script_filename << "' (around line " << current_line_number 
                   << ") for statement: " << statement_buffer;
        if (MinisqlParserGetError() != 0 && MinisqlParserGetErrorMessage() != nullptr) {
          std::cout << "Error (approx. line " << current_line_number << "): " << MinisqlParserGetErrorMessage() << std::endl;
        } else {
          std::cout << "Error in file [" << script_filename << "] (around line " << current_line_number << "): Syntax error in statement." << std::endl;
        }
        execution_status = DB_FAILED;
        DestroySyntaxTree();
        MinisqlParserFinish();
        break;
      }

      // 执行解析得到的 SQL 语句
      dberr_t statement_execution_result = Execute(statement_ast);
      
      DestroySyntaxTree();
      MinisqlParserFinish();

      if (statement_execution_result == DB_QUIT) {
        execution_status = DB_QUIT;
        std::cout << "QUIT command encountered in script. Halting script execution." << std::endl;
        break;
      }
      
      if (statement_execution_result != DB_SUCCESS) {
        LOG(WARNING) << "Error executing statement from file '" << script_filename << "' (around line " << current_line_number
                     << "): " << statement_buffer;
        execution_status = statement_execution_result;
        break;
      }
      
      statement_buffer.clear();
    }
  }

  script_file.close();

  // 输出执行结果摘要
  if (execution_status == DB_SUCCESS) {
    std::cout << "SQL script file [" << script_filename << "] executed successfully." << std::endl;
  } else if (execution_status != DB_QUIT) {
    std::cout << "Execution of SQL script file [" << script_filename << "] encountered errors." << std::endl;
  }
  
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteQuit(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteQuit" << std::endl;
#endif
  ExecuteInformation(DB_QUIT);
  return DB_QUIT;
}
