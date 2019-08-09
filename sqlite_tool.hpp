

#ifndef sqlite_tool_hpp
#define sqlite_tool_hpp

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <vector>
#include <deque>
#include <tuple>
#include <utility>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#define SQLXX_FILE_ACCESS _access
#define SQLXX_FILE_ENOENT (-1)//(ENOENT)
#define SQLXX_FILE_CREATE(path, mode) \
FILE *file;\
fopen_s(&file, path, mode);\
if (file != nullptr)\
{\
	fclose(file);\
}
#else
#include <unistd.h>
#define SQLXX_FILE_ACCESS access
#define SQLXX_FILE_ENOENT (-1)
#define SQLXX_FILE_CREATE(path, mode) \
FILE *file = fopen(path, mode);\
if (file != nullptr)\
{\
	fclose(file);\
}
#endif // _WIN64

#include "sqlite3.h"

namespace sqlite_tool {
    typedef std::string char_string;
    using any_mem_t = unsigned char;
    typedef std::basic_string<any_mem_t, std::char_traits<any_mem_t>, std::allocator<any_mem_t>> data_string;
    typedef sqlite3_int64 integer;
    typedef double real;
    
    class sqlite3_row {
    public:
        /**
         *memory offset; memory length; column index (start from 0); type;
         */
        typedef std::tuple<size_t, size_t, size_t, const std::type_info &> column_info;
    private:
        size_t row_mem_size = 0;
        unsigned char *data = nullptr;
        std::vector<sqlite3_row::column_info> row_info;
        
        void format_row_data() {
            memset(data, 0, row_mem_size);
            for (size_t index = 0; index < row_info.size(); index++) {
                const column_info &col_inf = row_info.at(index);
                if (std::get<3>(col_inf) == typeid(char_string)) {
                    new(data + std::get<0>(col_inf)) char_string();
                }
                else if (std::get<3>(col_inf) == typeid(data_string)) {
                    new(data + std::get<0>(col_inf)) data_string();
                }
            }
        }
        
        template<typename T>
        void
        delete_data(T *dataptr) {
            delete dataptr;
        }
        
        void delete_row_data() {
            if (data == nullptr) {
                return;
            }
            for (const column_info &col_inf : row_info) {
                size_t offset, length;
                std::tie(offset, length, std::ignore, std::ignore) = col_inf;
                const std::type_info &type = std::get<3>(col_inf);
                if (type == typeid(char_string)) {
                    void *buf = operator new(length);
                    memmove(buf, data + offset, length);
                    delete reinterpret_cast<char_string *>(buf);
                }
                else if (type == typeid(data_string)) {
                    void *buf = operator new(length);
                    memmove(buf, data + offset, length);
                    delete reinterpret_cast<data_string *>(buf);
                }
            }
            operator delete(data);
            data = nullptr;
            row_info.clear();
            row_mem_size = 0;
        }
    public:
        sqlite3_row() {
            
        }
        
        sqlite3_row(const std::vector<sqlite3_row::column_info> &info, size_t size) : row_info(info), row_mem_size(size) {
            data = reinterpret_cast<unsigned char *>(operator new(size));
            format_row_data();
        }
        
        sqlite3_row(std::vector<sqlite3_row::column_info> &&info, size_t size) : row_info(std::move(info)), row_mem_size(size) {
            data = reinterpret_cast<unsigned char *>(operator new(size));
            format_row_data();
        }
        
        ~sqlite3_row() {
            delete_row_data();
        }
        
        void sqlite3_row_rv(sqlite3_row &&obj) {
            std::swap(row_info, obj.row_info);
            std::swap(data, obj.data);
            std::swap(row_mem_size, obj.row_mem_size);
        }
        
        sqlite3_row(const sqlite3_row &) = delete;
        sqlite3_row(sqlite3_row &&obj) {
            sqlite3_row_rv(std::forward<sqlite3_row>(obj));
        }
        
        sqlite3_row &operator=(const sqlite3_row &) = delete;
        sqlite3_row &operator=(sqlite3_row &&obj) {
            sqlite3_row_rv(std::forward<sqlite3_row>(obj));
            return *this;
        }
        
        template<typename T>
        T get_column(size_t index) {
            const column_info &col_inf = row_info.at(index);
            if (std::get<1>(col_inf) != sizeof(T)) {
                throw ;
            }
            size_t offset = std::get<0>(col_inf);
            T t = *(reinterpret_cast<const T *>(data + offset));
            return std::move(t);
        }
        
        template<typename T>
        void set_column(size_t index, const T &t) {
            const column_info &col_inf = row_info.at(index);
            if (std::get<1>(col_inf) != sizeof(T)) {
                throw ;
            }
            size_t offset = std::get<0>(col_inf);
            *(reinterpret_cast<typename std::remove_reference<T>::type *>(data + offset)) = t;
        }
        
        template<typename T>
        void set_column(size_t index, T &&t) {
            const column_info &col_inf = row_info.at(index);
            if (std::get<1>(col_inf) != sizeof(T)) {
                throw ;
            }
            size_t offset = std::get<0>(col_inf);
            *(reinterpret_cast<typename std::remove_reference<T>::type *>(data + offset)) = std::move(t);
        }
    };
    
    class stmt_utility {
    public:
        void
        static stmt_get_column(sqlite3_stmt *stmtptr, size_t col, integer &val) {
            SQLITE_API sqlite3_int64 SQLITE_STDCALL value = sqlite3_column_int64(stmtptr, int(col));
            val = value;
        }
        
        void
        static stmt_get_column(sqlite3_stmt *stmtptr, size_t col, double &val) {
            SQLITE_API double SQLITE_STDCALL value = sqlite3_column_double(stmtptr, int(col));
            val = value;
        }
        
        void
        static stmt_get_column(sqlite3_stmt *stmtptr, size_t col, char_string &val) {
            SQLITE_API const unsigned char * SQLITE_STDCALL value = sqlite3_column_text(stmtptr, int(col));
            SQLITE_API int SQLITE_STDCALL bytes = sqlite3_column_bytes(stmtptr, int(col));
            char_string value_string(reinterpret_cast<const char *>(value), bytes);
            val = std::move(value_string);
        }
        
        void
        static stmt_get_column(sqlite3_stmt *stmtptr, size_t col, data_string &val) {
            SQLITE_API const void * SQLITE_STDCALL value = sqlite3_column_blob(stmtptr, int(col));
            SQLITE_API int SQLITE_STDCALL bytes = sqlite3_column_bytes(stmtptr, int(col));
            data_string value_string(reinterpret_cast<const unsigned char *>(value), bytes);
            val = std::move(value_string);
        }
        
        template<typename T>
        T
        static stmt_get_column(sqlite3_stmt *stmtptr, size_t col) {
            T t;
            stmt_get_column(stmtptr, col, t);
            return std::move(t);
        }
        
        template<typename T>
        void
        static stmt_get_column_to_row(sqlite3_stmt *stmtptr, size_t column, sqlite3_row &row) {
            SQLITE_API T SQLITE_STDCALL value;
            stmt_utility::stmt_get_column(stmtptr, column, value);
            row.set_column(column, std::move(value));
        }
    };
    
    class bind_utility {
    public:
        /**
         *bind with column name and value
         */
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<std::string, int> &column_value_pair) {
            std::string parameter_name("$");
            parameter_name.append(column_value_pair.first);
            SQLITE_API int SQLITE_STDCALL index = sqlite3_bind_parameter_index(stmt, parameter_name.c_str());
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_int(stmt, index, column_value_pair.second);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<std::string, sqlite3_int64> &column_value_pair) {
            std::string parameter_name("$");
            parameter_name.append(column_value_pair.first);
            SQLITE_API int SQLITE_STDCALL index = sqlite3_bind_parameter_index(stmt, parameter_name.c_str());
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_int64(stmt, index, column_value_pair.second);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<std::string, double> &column_value_pair) {
            std::string parameter_name("$");
            parameter_name.append(column_value_pair.first);
            SQLITE_API int SQLITE_STDCALL index = sqlite3_bind_parameter_index(stmt, parameter_name.c_str());
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_double(stmt, index, column_value_pair.second);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<std::string, char_string> &column_value_pair) {
            std::string parameter_name("$");
            parameter_name.append(column_value_pair.first);
            SQLITE_API int SQLITE_STDCALL index = sqlite3_bind_parameter_index(stmt, parameter_name.c_str());
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_text(stmt, index, column_value_pair.second.c_str(), int(column_value_pair.second.size()), SQLITE_TRANSIENT);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<std::string, data_string> &column_value_pair) {
            std::string parameter_name("$");
            parameter_name.append(column_value_pair.first);
            SQLITE_API int SQLITE_STDCALL index = sqlite3_bind_parameter_index(stmt, parameter_name.c_str());
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_blob(stmt, index, column_value_pair.second.data(), int(column_value_pair.second.size()), SQLITE_TRANSIENT);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<std::string, const sqlite3_value *> &column_value_pair) {
            std::string parameter_name("$");
            parameter_name.append(column_value_pair.first);
            SQLITE_API int SQLITE_STDCALL index = sqlite3_bind_parameter_index(stmt, parameter_name.c_str());
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_value(stmt, index, column_value_pair.second);
            return bind_err;
        }
        
        /**
         *bind with column index and value
         */
        /**
         * ^The second argument is the index of the SQL parameter to be set.
         * ^The leftmost SQL parameter has an index of 1.
         */
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<size_t, int> &column_value_pair) {
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_int(stmt, int(column_value_pair.first) + 1, column_value_pair.second);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<size_t, typename sqlite_tool::integer> &column_value_pair) {
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_int64(stmt, int(column_value_pair.first) + 1, column_value_pair.second);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<size_t, double> &column_value_pair) {
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_double(stmt, int(column_value_pair.first) + 1, column_value_pair.second);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<size_t, char_string> &column_value_pair) {
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_text(stmt, int(column_value_pair.first) + 1, column_value_pair.second.c_str(), int(column_value_pair.second.size()), SQLITE_TRANSIENT);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<size_t, data_string> &column_value_pair) {
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_blob(stmt, int(column_value_pair.first) + 1, column_value_pair.second.data(), int(column_value_pair.second.size()), SQLITE_TRANSIENT);
            return bind_err;
        }
        
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, const std::pair<size_t, const sqlite3_value *> &column_value_pair) {
            SQLITE_API int SQLITE_STDCALL bind_err = sqlite3_bind_value(stmt, int(column_value_pair.first) + 1, column_value_pair.second);
            return bind_err;
        }
        
        template<typename FT, typename ST, typename...RT>
        SQLITE_API int SQLITE_STDCALL 
        static bind_value(sqlite3_stmt *stmt, FT &&first, ST &&second, RT &&...rest) {
            SQLITE_API int SQLITE_STDCALL bind_err = bind_value(stmt, std::forward<FT>(first));
            if (bind_err != SQLITE_OK) {
                return bind_err;
            }
            return bind_value(stmt, std::forward<ST>(second), std::forward<RT>(rest)...);
        }
    };

    using Col_Nms_Type = std::vector<std::string>;
    using Col_Tps_Type = std::vector<std::string>;
    using Db_Row_Type = std::vector<sqlite3_row::column_info>;
    
	template<typename T>
	void
	make_column_constraint(Col_Tps_Type& column_constraints, Db_Row_Type& db_row, size_t& column_count, size_t& db_row_size) {
		throw;
	}
    //integer
    template<>
    void
	make_column_constraint<typename sqlite_tool::integer>(Col_Tps_Type& column_constraints, Db_Row_Type& db_row, size_t& column_count, size_t& db_row_size) {
		column_constraints.emplace_back(std::string("INTEGER DEFAULT 0"));
		const std::type_info& col_tp = typeid(sqlite_tool::integer);
		auto&& col_tuple = sqlite3_row::column_info(db_row_size, sizeof(sqlite_tool::integer), column_count, col_tp);
		db_row.emplace_back(std::move(col_tuple));
		db_row_size += sizeof(sqlite_tool::integer);
		column_count++;
	}
    //real
    template<>
    void
	make_column_constraint<typename sqlite_tool::real>(Col_Tps_Type& column_constraints, Db_Row_Type& db_row, size_t& column_count, size_t& db_row_size) {
		column_constraints.emplace_back(std::string("REAL DEFAULT 0.0"));
		const std::type_info& col_tp = typeid(sqlite_tool::real);
		auto&& col_tuple = sqlite3_row::column_info(db_row_size, sizeof(sqlite_tool::real), column_count, col_tp);
		db_row.emplace_back(std::move(col_tuple));
		db_row_size += sizeof(sqlite_tool::real);
		column_count++;
	}
    //char_string
    template<>
    void
	make_column_constraint<typename sqlite_tool::char_string>(Col_Tps_Type& column_constraints, Db_Row_Type& db_row, size_t& column_count, size_t& db_row_size) {
		column_constraints.emplace_back(std::string("TEXT DEFAULT \"\""));
		const std::type_info& col_tp = typeid(sqlite_tool::char_string);
		auto&& col_tuple = sqlite3_row::column_info(db_row_size, sizeof(sqlite_tool::char_string), column_count, col_tp);
		db_row.emplace_back(std::move(col_tuple));
		db_row_size += sizeof(sqlite_tool::char_string);
		column_count++;
	}
    //data_string
    template<>
    void
	make_column_constraint<typename sqlite_tool::data_string>(Col_Tps_Type& column_constraints, Db_Row_Type& db_row, size_t& column_count, size_t& db_row_size) {
		column_constraints.emplace_back(std::string("BLOB NOT NULL"));
		const std::type_info& col_tp = typeid(sqlite_tool::data_string);
		auto&& col_tuple = sqlite3_row::column_info(db_row_size, sizeof(sqlite_tool::data_string), column_count, col_tp);
		db_row.emplace_back(std::move(col_tuple));
		db_row_size += sizeof(sqlite_tool::data_string);
		column_count++;
	}
    
    template<typename...COLUMN_TYPE>
    class sqlite3_delegate {        
    private:
        template <typename...T>
        using tp_type = std::tuple<T...>;
        typedef tp_type<COLUMN_TYPE...> full_tuple_type;
        
        /**
         *member variables
         */
        sqlite_tool::Col_Nms_Type columns;
        sqlite_tool::Col_Tps_Type column_constraints;
        std::string db_file;
        std::string table;
        struct sqlite3 *sqdb = nullptr;
        size_t db_row_size = 0;
        size_t column_count = 0;
        sqlite_tool::Db_Row_Type db_row;
        
        std::string execute_conditions;
    private:
        void 
        push_col_name(std::vector<std::string> &columns, std::string &&column) {
            columns.push_back(column);
        }
        
        template<typename FT, typename ST, typename...RT>
        void 
        push_col_name(std::vector<std::string> &columns, FT &&first, ST &&second, RT &&...rest) {
            push_col_name(columns, std::forward<FT>(first));
            push_col_name(columns, std::forward<ST>(second), std::forward<RT>(rest)...);
        }
        
        template<typename T>
        void
        init_column_constraints() {
			make_column_constraint<T>(column_constraints, db_row, column_count, db_row_size);
        }
        
        template<typename T1, typename T2, typename...Tn>
        void
        init_column_constraints() {
            init_column_constraints<T1>();
            init_column_constraints<T2, Tn...>();
        }
        
    public:
        
        sqlite3_delegate() {
            init_column_constraints<COLUMN_TYPE...>();
        }
        
        ~sqlite3_delegate() {
            if (sqdb) {
                sqlite3_close(sqdb);
                sqdb = nullptr;
            }
        }
        
        void set_db_file_path(std::string db) {
            db_file = std::move(db);
        }
        
        void set_table_name(std::string table) {
            this->table = std::move(table);
        }
        
        template<typename...T>
        void
        set_column_names(T...t) {
            if (sizeof...(COLUMN_TYPE) != sizeof...(T)) {
                throw;
            }
            columns.clear();
            push_col_name(columns, std::forward<T>(t)...);
        }

    private:
        void
        push_column_constraint(std::string constraint) {
            column_constraints.emplace_back(constraint);
        }

        template<typename T0, typename T1, typename...T>
        void 
        push_column_constraint(T0 t0, T1 t1, T...t) {
            push_column_constraint(std::forward<T0>(t0));
            push_column_constraint(std::forward<T1>(t1), std::forward<T>(t)...);
        }
    public:
        template<typename...T>
        void
        set_column_constraints(T...t) {
            if (sizeof...(COLUMN_TYPE) != sizeof...(T)) {
                throw;
            }
            column_constraints.clear();
            push_column_constraint(std::forward<T>(t)...);
        }
        
        template<std::size_t index>
        void
        add_column_constraint(std::string constraint) {
//            std::swap(column_constraints.at(index), constraint);
            column_constraints.at(index).append(" ");
            column_constraints.at(index).append(constraint);
        }

        template<std::size_t index>
        void 
        set_column_constraint(std::string constraint) {
            column_constraints.at(index) = constraint;
        }

        SQLITE_API int SQLITE_STDCALL 
        open_db() {
            //return sqlite3_open_v2(db_file.c_str(), &sqdb, SQLITE_OPEN_READWRITE, NULL);
			return sqlite3_open(db_file.c_str(), &sqdb);
        }
        
        SQLITE_API int SQLITE_STDCALL
        create_table_if_not_exists() {
			auto err = SQLXX_FILE_ACCESS(db_file.c_str(), 00);
            if (err == SQLXX_FILE_ENOENT) {
                SQLXX_FILE_CREATE(db_file.c_str(), "a");
            }
            
            std::string sqlcmd("CREATE TABLE IF NOT EXISTS ");
            sqlcmd.append(table);
            sqlcmd.append("(");
            for (size_t index = 0; index < columns.size(); index++) {
                const std::string &name = columns.at(index);
                const std::string &type = column_constraints.at(index);
                sqlcmd.append(name);
                sqlcmd.append(" ");
                sqlcmd.append(type);
                if (name != columns.back()) {
                    sqlcmd.append(",");
                }
            }
            sqlcmd.append(")");
            
            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return open_err;
                }
            }

            sqlite3_stmt *stmt = nullptr;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(-1), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return prep_err;
            }
            
            SQLITE_API int SQLITE_STDCALL step_err = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (step_err != SQLITE_DONE) {
                return step_err;
            }
            
            return SQLITE_OK;
        }
        
    private:
        template<typename VALUE>
        void
        insert_command_prepare_name(std::string &sqlcmd, const std::pair<std::string, VALUE> &column_value_pair) {
            sqlcmd.append(column_value_pair.first);
        }
        
        template<typename VALUE>
        void
        insert_command_prepare_name(std::string &sqlcmd, const std::pair<size_t, VALUE> &column_value_pair) {
            const std::string &column = columns.at(column_value_pair.first);
            sqlcmd.append(column);
        }
        
        template<typename FT, typename ST, typename...RT>
        void
        insert_command_prepare_name(std::string &sqlcmd, FT &&first, ST &&second, RT &&...rest) {
            insert_command_prepare_name(sqlcmd, std::forward<FT>(first));
            sqlcmd.append(",");
            insert_command_prepare_name(sqlcmd, std::forward<ST>(second), std::forward<RT>(rest)...);
        }
        
        template<typename T>
        void
        insert_command_prepare_value(std::string &sqlcmd, const std::pair<std::string, T> &column_value_pair) {
            sqlcmd.append("$");
            sqlcmd.append(column_value_pair.first);
        }
        
        template<typename VALUE>
        void
        insert_command_prepare_value(std::string &sqlcmd, const std::pair<size_t, VALUE> &column_value_pair) {
            sqlcmd.append("?");
        }
        
        template<typename FT, typename ST, typename...RT>
        void
        insert_command_prepare_value(std::string &sqlcmd, FT &&first, ST &&second, RT &&...rest) {
            insert_command_prepare_value(sqlcmd, std::forward<FT>(first));
            sqlcmd.append(",");
            insert_command_prepare_value(sqlcmd, std::forward<ST>(second), std::forward<RT>(rest)...);
        }
        
    private:
        template<typename T>
        std::pair<std::string, T>
        bind_column_index_transfer_to_name(const std::pair<size_t, T> &pair) {
            return std::make_pair(columns.at(pair.first), pair.second);
        }
        
        template<typename T>
        std::pair<std::string, T>
        bind_column_index_transfer_to_name(const std::pair<std::string, T> &pair) {
            return pair;
        }
        
        template<typename T>
        std::pair<size_t, T>
        bind_column_index_transfer(const std::pair<size_t, T> &pair, size_t &index) {
            return std::make_pair(index++, pair.second);
        }
        
        template<typename T>
        std::pair<size_t, T>
        bind_column_index_transfer(const std::pair<std::string, T> &pair, size_t &index) {
            return std::make_pair(index++, pair.second);
        }

    public:
        template<typename COLTP, typename...VALTP>
        SQLITE_API int SQLITE_STDCALL
        put_row(std::pair<COLTP, VALTP>...pair) {
            size_t parameters = sizeof...(VALTP);
            if (parameters > std::tuple_size<full_tuple_type>::value) {
                return SQLITE_ERROR;
            }
            
            std::string sqlcmd("INSERT INTO ");
            sqlcmd.append(table);
            sqlcmd.append("(");
            insert_command_prepare_name(sqlcmd, pair...);
            sqlcmd.append(") VALUES(");
            //insert_command_prepare_value(sqlcmd, std::forward<std::pair<COLTP, VALTP>>(pair)...);
            insert_command_prepare_value(sqlcmd, bind_column_index_transfer_to_name(pair)...);
            sqlcmd.append(")");
            
            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return open_err;
                }
            }
            
            sqlite3_stmt *stmt = nullptr;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(sqlcmd.size()), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return prep_err;
            }
            
            SQLITE_API int SQLITE_STDCALL bind_err = bind_utility::bind_value(stmt, bind_column_index_transfer_to_name(pair)...);
            if (bind_err != SQLITE_OK) {
                sqlite3_clear_bindings(stmt);
                sqlite3_finalize(stmt);
                return bind_err;
            }
            
            SQLITE_API int SQLITE_STDCALL step_err = sqlite3_step(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_finalize(stmt);
            if (step_err != SQLITE_DONE) {
                return step_err;
            }
            
            return SQLITE_OK;
        }
        
    public:
        /**
         *get columns value uses dynamic run-time typing not static
         */
        SQLITE_API int SQLITE_STDCALL
        get_all(std::deque<sqlite3_row> &result) {
            std::string sqlcmd("SELECT * FROM ");
            sqlcmd.append(table);
            
            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return open_err;
                }
            }
            
            sqlite3_stmt *stmt = nullptr;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(sqlcmd.size()), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return prep_err;
            }
            
            SQLITE_API int SQLITE_STDCALL step_err = SQLITE_OK;
            while ((step_err = sqlite3_step(stmt)) == SQLITE_ROW) {
                sqlite3_row current_row(db_row, db_row_size);
                SQLITE_API int SQLITE_STDCALL col_size = sqlite3_column_count(stmt);
                /**^The leftmost column of the result set has the index 0.
                 */
                for (int col_idx = 0; col_idx < col_size; col_idx++) {
                    switch (sqlite3_column_type(stmt, col_idx)) {
                        case SQLITE_INTEGER: {
                            stmt_utility::stmt_get_column_to_row<sqlite_tool::integer>(stmt, col_idx, current_row);
                            break;
                        }
                        case SQLITE_FLOAT: {
                            stmt_utility::stmt_get_column_to_row<sqlite_tool::real>(stmt, col_idx, current_row);
                            break;
                        }
                        case SQLITE_BLOB: {
                            stmt_utility::stmt_get_column_to_row<sqlite_tool::data_string>(stmt, col_idx, current_row);
                            break;
                        }
                        case SQLITE_TEXT: {
                            stmt_utility::stmt_get_column_to_row<sqlite_tool::char_string>(stmt, col_idx, current_row);
                            break;
                        }
                        case SQLITE_NULL: {
                            const char *decl_type = sqlite3_column_decltype(stmt, col_idx);
                            if (strcmp(decl_type, "TEXT") == 0) {
                                current_row.set_column(col_idx, char_string(""));
                            }
                            else if (strcmp(decl_type, "INTEGER") == 0) {
                                current_row.set_column(col_idx, sqlite3_int64(0));
                            }
                            else if (strcmp(decl_type, "REAL") == 0) {
                                current_row.set_column(col_idx, double(0.0));
                            }
                            else if (strcmp(decl_type, "BLOB") == 0) {
                                unsigned char nval = 0;
                                current_row.set_column(col_idx, data_string(&nval, 1));
                            }
                            else {
                                throw ;
                            }
                            break;
                        }
                        default:
                            break;
                    }//switch
                }//for
                result.emplace_back(std::move(current_row));
            }//while
            sqlite3_finalize(stmt);
            if (step_err == SQLITE_DONE) {
                return SQLITE_OK;
            }
            else {
                return step_err;
            }
        }
        
    public:
        /**
         *get columns value uses static not dynamic run-time typing
         */
        template<size_t...col_x>
        std::deque<std::tuple<typename std::tuple_element<col_x, full_tuple_type>::type...>>
        get_column_value() {
            std::deque<std::tuple<typename std::tuple_element<col_x, full_tuple_type>::type...>> return_queue;
            std::string sqlcmd("SELECT * FROM ");
            sqlcmd.append(table);
            
            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return return_queue;
                }
            }
            
            sqlite3_stmt *stmt;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(sqlcmd.size()), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return return_queue;
            }
            
            SQLITE_API int SQLITE_STDCALL step_err = SQLITE_OK;
            while ((step_err = sqlite3_step(stmt)) == SQLITE_ROW) {
                auto &&row = std::make_tuple(stmt_utility::stmt_get_column<typename std::tuple_element<col_x, full_tuple_type>::type>(stmt, col_x)...);
                return_queue.emplace_back(std::move(row));
            }
            
            sqlite3_finalize(stmt);
            if (step_err != SQLITE_DONE) {
                //throw ;
            }
            
            return return_queue;
        }
        
        template<size_t...col_x>
        std::deque<std::tuple<typename std::tuple_element<col_x, full_tuple_type>::type...>>
        get_column_value_match_conditions() {
            std::deque<std::tuple<typename std::tuple_element<col_x, full_tuple_type>::type...>> return_queue;
            std::string sqlcmd("SELECT * FROM ");
            sqlcmd.append(table);
            sqlcmd.append(" WHERE ");
            sqlcmd.append(execute_conditions);
            
            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return return_queue;
                }
            }
            
            sqlite3_stmt *stmt;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(sqlcmd.size()), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return return_queue;
            }
            
            SQLITE_API int SQLITE_STDCALL step_err = SQLITE_OK;
            while ((step_err = sqlite3_step(stmt)) == SQLITE_ROW) {
                auto &&row = std::make_tuple(stmt_utility::stmt_get_column<typename std::tuple_element<col_x, full_tuple_type>::type>(stmt, col_x)...);
                return_queue.emplace_back(std::move(row));
            }
            
            sqlite3_finalize(stmt);
            if (step_err != SQLITE_DONE) {
                //throw ;
            }
            
            return return_queue;
        }

    public:
        SQLITE_API int SQLITE_STDCALL
        delete_rows_match_conditions() {
            std::string sqlcmd("DELETE FROM ");
            sqlcmd.append(table);
            sqlcmd.append(" WHERE ");
            sqlcmd.append(execute_conditions);

            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return open_err;
                }
            }

            sqlite3_stmt *stmt = nullptr;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(sqlcmd.size()), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return prep_err;
            }

            SQLITE_API int SQLITE_STDCALL step_err = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (step_err != SQLITE_DONE) {
                return step_err;
            }
            
            return SQLITE_OK;
        }

    private:
        void
        append_delete_condition_and(std::string &sqlcmd, std::string &&condition) {
            sqlcmd.append(condition);
        }

        template<typename T0, typename T1, typename...TN>
        void
        append_delete_condition_and(std::string &sqlcmd, T0 &&cond0, T1 &&cond1, TN &&...condn) {
            append_delete_condition_and(sqlcmd, std::forward<T0>(cond0));
            sqlcmd.append(" AND ");
            append_delete_condition_and(sqlcmd, std::forward<T1>(cond1), std::forward<TN>(condn)...);
        }

        void
        append_delete_condition_or(std::string &sqlcmd, std::string &&condition) {
            sqlcmd.append(condition);
        }

        template<typename T0, typename T1, typename...TN>
        void
        append_delete_condition_or(std::string &sqlcmd, T0 &&cond0, T1 &&cond1, TN &&...condn) {
            append_delete_condition_or(sqlcmd, std::forward<T0>(cond0));
            sqlcmd.append(" OR ");
            append_delete_condition_or(sqlcmd, std::forward<T1>(cond1), std::forward<TN>(condn)...);
        }
        
    public:
        template<typename T0, typename...TN>
        void
        set_conditions_match_all(T0 condition, TN...condn) {
            execute_conditions.clear();
            append_delete_condition_and(execute_conditions, std::forward<T0>(condition), std::forward<TN>(condn)...);
        }
        
        template<typename T0, typename...TN>
        void
        set_conditions_match_any(T0 condition, TN...condn) {
            execute_conditions.clear();
            append_delete_condition_or(execute_conditions, std::forward<T0>(condition), std::forward<TN>(condn)...);
        }
        
    private:
        template<typename T>
        void
        update_prepare_name_value(std::string &sqlcmd, const std::pair<size_t, T> &pair) {
            const std::string &column_name = columns.at(pair.first);
            sqlcmd.append(column_name);
            sqlcmd.append("=$");
            sqlcmd.append(column_name);
        }
        
        template<typename FT, typename ST, typename...RT>
        void
        update_prepare_name_value(std::string &sqlcmd, const std::pair<size_t, FT> &first, const std::pair<size_t, ST> &second, const std::pair<size_t, RT> &...rest) {
            update_prepare_name_value(std::forward<std::string &>(sqlcmd), std::forward<const std::pair<size_t, FT>>(first));
            sqlcmd.append(",");
            update_prepare_name_value(std::forward<std::string &>(sqlcmd), std::forward<const std::pair<size_t, ST>>(second), std::forward<const std::pair<size_t, RT>>(rest)...);
        }
        
        template<typename T>
        void
        update_prepare_name_value(std::string &sqlcmd, const std::pair<std::string, T> &pair) {
            const std::string &column_name = pair.first;
            sqlcmd.append(column_name);
            sqlcmd.append("=$");
            sqlcmd.append(column_name);
        }
        
        template<typename FT, typename ST, typename...RT>
        void
        update_prepare_name_value(std::string &sqlcmd, const std::pair<std::string, FT> &first, const std::pair<std::string, ST> &second, const std::pair<std::string, RT> &...rest) {
            update_prepare_name_value(std::forward<std::string &>(sqlcmd), std::forward<const std::pair<std::string, FT>>(first));
            sqlcmd.append(",");
            update_prepare_name_value(std::forward<std::string &>(sqlcmd), std::forward<const std::pair<std::string, ST>>(second), std::forward<std::pair<std::string, RT>>(rest)...);
        }
        
    public:
        template<typename COLTP, typename...VALTP>
        SQLITE_API int SQLITE_STDCALL
        update_column_value_match_conditions(std::pair<COLTP, VALTP>...pair) {
            std::string sqlcmd("UPDATE ");
            sqlcmd.append(table);
            sqlcmd.append(" SET ");
            update_prepare_name_value(sqlcmd, std::forward<std::pair<COLTP, VALTP>>(pair)...);
            sqlcmd.append(" WHERE ");
            sqlcmd.append(execute_conditions);
            
            if (sqdb == nullptr) {
                SQLITE_API int SQLITE_STDCALL open_err = open_db();
                if (open_err != SQLITE_OK) {
                    return open_err;
                }
            }
            
            sqlite3_stmt *stmt = nullptr;
            SQLITE_API int SQLITE_STDCALL prep_err = sqlite3_prepare_v2(sqdb, sqlcmd.c_str(), int(sqlcmd.size()), &stmt, NULL);
            if (prep_err != SQLITE_OK) {
                return prep_err;
            }
            
            SQLITE_API int SQLITE_STDCALL bind_err = bind_utility::bind_value(stmt, std::forward<std::pair<std::string, VALTP>>(bind_column_index_transfer_to_name(pair))...);
            if (bind_err != SQLITE_OK) {
                sqlite3_clear_bindings(stmt);
                sqlite3_finalize(stmt);
                return bind_err;
            }
            
            SQLITE_API int SQLITE_STDCALL step_err = sqlite3_step(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_finalize(stmt);
            if (step_err != SQLITE_DONE) {
                return step_err;
            }
            
            return SQLITE_OK;
        }
    };
}


#endif /* sqlite_tool_hpp */
