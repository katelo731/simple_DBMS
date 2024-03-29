#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "Util.h"
#include "Command.h"
#include "Table.h"

///
/// Allocate State_t and initialize some attributes
/// Return: ptr of new State_t
///
State_t* new_State() {
    State_t *state = (State_t*)malloc(sizeof(State_t));
    state->saved_stdout = -1;
    return state;
}

///
/// Print shell prompt
///
void print_prompt(State_t *state) {
    if (state->saved_stdout == -1) {
        printf("db > ");
    }
}

///
/// Print the user in the specific format
///
void print_user(User_t *user) {
    printf("(%d, %s, %s, %d)\n", user->id, user->name, user->email, user->age);
}

//
// function for task 4
//
void print_user_proj(Command_t *cmd, User_t *user) {
    int cmd_ptr = 1;
    while(strncmp(cmd->args[cmd_ptr], "from", 4)){
        if(!strncmp(cmd->args[cmd_ptr], "*", 1)){
            printf("(%d, %s, %s, %d)\n", user->id, user->name, user->email, user->age);
            return;
        }
        if(cmd_ptr == 1) printf("(");
        else printf(", ");
        if(!strncmp(cmd->args[cmd_ptr], "id", 2))
            printf("%d", user->id);
        else if(!strncmp(cmd->args[cmd_ptr], "name", 4))
            printf("%s", user->name);
        else if(!strncmp(cmd->args[cmd_ptr], "email", 5))
            printf("%s", user->email);
        else if(!strncmp(cmd->args[cmd_ptr], "age", 3))
            printf("%d", user->age);
        cmd_ptr += 1;
    }
    printf(")\n");
}

///
/// This function received an output argument
/// Return: category of the command
///
int parse_input(char *input, Command_t *cmd) {
    char *token;
    int idx;
    token = strtok(input, " \n");
    for (idx = 0; strlen(cmd_list[idx].name) != 0; idx++) {
        if (!strncmp(token, cmd_list[idx].name, cmd_list[idx].len)) {
            cmd->type = cmd_list[idx].type;
        }
    }
    while (token != NULL) {
        add_Arg(cmd, token);
        token = strtok(NULL, " \n");
    }
    return cmd->type;
}

///
/// Handle built-in commands
/// Return: command type
///
void handle_builtin_cmd(Table_t *table, Command_t *cmd, State_t *state) {
    if (!strncmp(cmd->args[0], ".exit", 5)) {
        archive_table(table);
        exit(0);
    } else if (!strncmp(cmd->args[0], ".output", 7)) {
        if (cmd->args_len == 2) {
            if (!strncmp(cmd->args[1], "stdout", 6)) {
                close(1);
                dup2(state->saved_stdout, 1);
                state->saved_stdout = -1;
            } else if (state->saved_stdout == -1) {
                int fd = creat(cmd->args[1], 0644);
                state->saved_stdout = dup(1);
                if (dup2(fd, 1) == -1) {
                    state->saved_stdout = -1;
                }
                __fpurge(stdout); //This is used to clear the stdout buffer
            }
        }
    } else if (!strncmp(cmd->args[0], ".load", 5)) {
        if (cmd->args_len == 2) {
            load_table(table, cmd->args[1]);
        }
    } else if (!strncmp(cmd->args[0], ".help", 5)) {
        print_help_msg();
    }
}

///
/// Handle query type commands
/// Return: command type
///
int handle_query_cmd(Table_t *table, Command_t *cmd) {
    if (!strncmp(cmd->args[0], "insert", 6)) {
        handle_insert_cmd(table, cmd);
        return INSERT_CMD;
    } else if (!strncmp(cmd->args[0], "select", 6)) {
        handle_select_cmd(table, cmd);
        return SELECT_CMD;
    } else {
        return UNRECOG_CMD;
    }
}

///
/// The return value is the number of rows insert into table
/// If the insert operation success, then change the input arg
/// `cmd->type` to INSERT_CMD
///
int handle_insert_cmd(Table_t *table, Command_t *cmd) {
    int ret = 0;
    User_t *user = command_to_User(cmd);
    if (user) {
        ret = add_User(table, user);
        if (ret > 0) {
            cmd->type = INSERT_CMD;
        }
    }
    return ret;
}

///
/// The return value is the number of rows select from table
/// If the select operation success, then change the input arg
/// `cmd->type` to SELECT_CMD
///
int handle_select_cmd(Table_t *table, Command_t *cmd) {
    size_t idx;
    int offset_flag = 0;
    int offset_num = 0;
    int limit_flag = 0;
    int limit_num = 0;

    // empty table
    if(table->len == 0){
        return 0;
    }
    // 'select'
    if(cmd->args_len == 1){
        for (idx = 0; idx < table->len; idx++) {
            print_user(get_User(table, idx));
        }
        cmd->type = SELECT_CMD;
        return table->len; 
    }
    // 'select limit <limit_num>' 
    else if(!strncmp(cmd->args[1], "limit", 5)){
        //
        // Limit argument
	// Implement limit argument to restrict 
        // the number of output
        //
	if(!cmd->args[2]) return 0;
        size_t show_num = atoi(cmd->args[2]);
        for (idx = 0; idx < show_num && idx < table->len; idx++) {
            print_user(get_User(table, idx));
        }
        cmd->type = SELECT_CMD;
        return (show_num < table->len? show_num : table->len);
    } 
    // 'select offset <offset_num>'
    else if(!strncmp(cmd->args[1], "offset", 6)){
        //
        // Offset argument
	// Implement offset argument to add offset
	// for the query result
        //
	if(!cmd->args[2]) return 0;
        size_t offset = atoi(cmd->args[2]);
        for (idx = offset; idx < table->len; idx++) {
            print_user(get_User(table, idx));
        }
        cmd->type = SELECT_CMD;
        return ((table->len - offset) < 0? 0 : (table->len - offset));
    }
    // 'select */id/name/email/age from <table_name> [offset <offset_num>] [limit <limit_num>]' 
    else if((!strncmp(cmd->args[1], "id", 2)) 
	      || (!strncmp(cmd->args[1], "name", 4))
	      || (!strncmp(cmd->args[1], "email", 5))
	      || (!strncmp(cmd->args[1], "age", 3))
	      || (!strncmp(cmd->args[1], "*", 1))){
        //
        // Projection (field selection)
	// Implement projection(field selection) 
        // in select query
        // 	

        // offset detection
        if(cmd->args_len >= 5){
	    if(!strncmp(cmd->args[cmd->args_len-4], "offset", 6)){
	        offset_flag = 1;
	        offset_num = atoi(cmd->args[cmd->args_len-3]);
	    } else if(!strncmp(cmd->args[cmd->args_len-2], "offset", 6)){
	        offset_flag = 1;
	        offset_num = atoi(cmd->args[cmd->args_len-1]);
	    }
        } else if(cmd->args_len >= 3){
	    if(!strncmp(cmd->args[cmd->args_len-2], "offset", 6)){
	        offset_flag = 1;
	        offset_num = atoi(cmd->args[cmd->args_len-1]);
	    }
        }

        // limit detection
        if(cmd->args_len >= 5){
	    if(!strncmp(cmd->args[cmd->args_len-4], "limit", 5)){
	        limit_flag = 1;
	        limit_num = atoi(cmd->args[cmd->args_len-3]);
	    } else if(!strncmp(cmd->args[cmd->args_len-2], "limit", 5)){
	        limit_flag = 1;
	        limit_num = atoi(cmd->args[cmd->args_len-1]);
	    }
        } else if(cmd->args_len >= 3){
	    if(!strncmp(cmd->args[cmd->args_len-2], "limit", 5)){
	        limit_flag = 1;
	        limit_num = atoi(cmd->args[cmd->args_len-1]);
	    }
        }

	int c = 0;
        for (idx = 0; idx < table->len; idx++) {
	    if(offset_flag && offset_num > 0){
	        offset_num --;
  	        continue;
	    }
	    if(limit_flag && limit_num > 0){
		limit_num --;
		print_user_proj(cmd, get_User(table, idx));
		c++;
	    } else if(!limit_flag){
            	print_user_proj(cmd, get_User(table, idx));
		c++;
	    }
        }

        cmd->type = SELECT_CMD;
        return c;
    }
    // 'select <invalid_instr>' 
    else {
        return 0;
    }
}

///
/// Show the help messages
///
void print_help_msg() {
    const char msg[] = "# Supported Commands\n"
    "\n"
    "## Built-in Commands\n"
    "\n"
    "  * .exit\n"
    "\tThis cmd archives the table, if the db file is specified, then exit.\n"
    "\n"
    "  * .output\n"
    "\tThis cmd change the output strategy, default is stdout.\n"
    "\n"
    "\tUsage:\n"
    "\t    .output (<file>|stdout)\n\n"
    "\tThe results will be redirected to <file> if specified, otherwise they will display to stdout.\n"
    "\n"
    "  * .load\n"
    "\tThis command loads records stored in <DB file>.\n"
    "\n"
    "\t*** Warning: This command will overwrite the records already stored in current table. ***\n"
    "\n"
    "\tUsage:\n"
    "\t    .load <DB file>\n\n"
    "\n"
    "  * .help\n"
    "\tThis cmd displays the help messages.\n"
    "\n"
    "## Query Commands\n"
    "\n"
    "  * insert\n"
    "\tThis cmd inserts one user record into table.\n"
    "\n"
    "\tUsage:\n"
    "\t    insert <id> <name> <email> <age>\n"
    "\n"
    "\t** Notice: The <name> & <email> are string without any whitespace character, and maximum length of them is 255. **\n"
    "\n"
    "  * select\n"
    "\tThis cmd will display all user records in the table.\n"
    "\n";
    printf("%s", msg);
}

