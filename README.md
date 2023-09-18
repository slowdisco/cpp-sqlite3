# cpp-sqlite3



    //初始化sqlite3_delegate对象，模板参数为数据库表每列的数据类型(column type)

    sqlite_tool::sqlite3_delegate<sqlite_tool::integer, sqlite_tool::real, sqlite_tool::char_string, sqlite_tool::data_string> sq_delegate;
    
    //设置数据库文件路径
    
    sq_delegate.set_db_file_path(std::string("your db file path"));
    
    //设置数据库表(table)名称
    
    sq_delegate.set_table_name(std::string("your table name"));
    
    //设置表中每一列(column)的名称
    
    sq_delegate.set_column_names(std::string("int_col"), std::string("real_col"), std::string("text_col"), std::string("blob_col"));
    
    //创建表
    
    sq_delegate.create_table_if_not_exists();
    
    //通过表中列的序号(column index)插入数据
    
    unsigned char blob_data[128];
    sq_delegate.put_row(std::make_pair(size_t(0), sqlite_tool::integer(1)),
                        std::make_pair(size_t(1), sqlite_tool::real(0.1)),
                        std::make_pair(size_t(2), sqlite_tool::char_string("a text")),
                        std::make_pair(size_t(3), sqlite_tool::data_string(blob_data, sizeof blob_data)));
                        
    //通过表中列的名称(column name)插入数据
    
    sq_delegate.put_row(std::make_pair(std::string("int_col"), sqlite_tool::integer(2)),
                        std::make_pair(std::string("real_col"), sqlite_tool::real(0.2)),
                        std::make_pair(std::string("text_col"), sqlite_tool::char_string("another text")),
                        std::make_pair(std::string("blob_col"), sqlite_tool::data_string(blob_data, sizeof blob_data)));
    //按条件删除数据
    //设置删除条件
    //有多个条件时，要删除满足全部条件的条目，通过此方法设置
    
    sq_delegate.set_conditions_match_all(std::string("int_col>0"), std::string("int_col<2"));
    
    //有多个条件时，要删除满足任一条件的条目，通过此方法设置
    
    sq_delegate.set_conditions_match_any(std::string("int_col=1"), std::string("int_col=2"));
    
    //执行按设定条件删除
    
    sq_delegate.delete_rows_match_conditions();
    
    //按条件查找数据
    //设置查找条件
    //有多个条件时，要查找满足全部条件的条目，通过此方法设置
    
    sq_delegate.set_conditions_match_all(std::string("int_col=1"), std::string("real_col=0.1"));
    
    //有多个条件时，要查找满足任一条件的条目，通过此方法设置
    
    sq_delegate.set_conditions_match_any(std::string("int_col=1"), std::string("int_col=2"));
    
    //执行按设定条件查找
    
    /*
     *模板参数为需要获取的列的序号(column index)，如果只需要返回表中第M和N列数据(M, N 从0开始)：
     *sq_delegate.get_column_value_match_conditions<M, N>();
     */
     
    auto result = sq_delegate.get_column_value_match_conditions<0, 1, 2, 3>();
    
    //获取查找到的数据(以获取结果中的第一条为例)
    
    auto item_0_tuple = result.at(0);
    sqlite_tool::integer int_value = std::get<0>(item_0_tuple);
    sqlite_tool::real real_value = std::get<1>(item_0_tuple);
    sqlite_tool::char_string text_value = std::get<2>(item_0_tuple);
    sqlite_tool::data_string blob_value = std::get<3>(item_0_tuple);
    
    //获取所有数据
    
    auto all_result = sq_delegate.get_column_value<0, 1, 2, 3>();
