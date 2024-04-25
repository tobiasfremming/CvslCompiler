#include "vslc.h"

// This header defines a bunch of macros we can use to emit assembly to stdout
#include "emit.h"

// In the System V calling convention, the first 6 integer parameters are passed in registers
#define NUM_REGISTER_PARAMS 6
static const char *REGISTER_PARAMS[6] = {RDI, RSI, RDX, RCX, R8, R9};

// Takes in a symbol of type SYMBOL_FUNCTION, and returns how many parameters the function takes
#define FUNC_PARAM_COUNT(func) ((func)->node->children[1]->n_children)

static void generate_stringtable(void);
static void generate_global_variables(void);
static void generate_function(symbol_t *function);
static void generate_expression(node_t *expression);
static void generate_statement(node_t *node);
static void generate_main(symbol_t *first);
static char *get_jump_operator(char *operator);

static size_t if_label_counter;
static size_t while_label_counter;
static size_t break_loop_id;

/* Entry point for code generation */
void generate_program(void)
{
    generate_stringtable();
    generate_global_variables();
    if_label_counter = 0;
    while_label_counter = 0;
    break_loop_id = 0;

    DIRECTIVE(".text");
    symbol_t *first_function = NULL;
    for (size_t i = 0; i < global_symbols->n_symbols; i++)
    {
        symbol_t *symbol = global_symbols->symbols[i];
        if (symbol->type != SYMBOL_FUNCTION)
            continue;
        if (!first_function)
            first_function = symbol;
        generate_function(symbol);
    }

    if (first_function == NULL)
    {
        fprintf(stderr, "error: program contained no functions\n");
        exit(EXIT_FAILURE);
    }
    generate_main(first_function);
}

/* Prints one .asciz entry for each string in the global string_list */
static void generate_stringtable(void)
{
    DIRECTIVE(".section %s", ASM_STRING_SECTION);
    // These strings are used by printf
    DIRECTIVE("intout: .asciz \"%s\"", "%ld");
    DIRECTIVE("strout: .asciz \"%s\"", "%s");
    // This string is used by the entry point-wrapper
    DIRECTIVE("errout: .asciz \"%s\"", "Wrong number of arguments");

    for (size_t i = 0; i < string_list_len; i++)
        DIRECTIVE("string%ld: \t.asciz %s", i, string_list[i]);
}

/* Prints .zero entries in the .bss section to allocate room for global variables and arrays */
static void generate_global_variables(void)
{
    DIRECTIVE(".section %s", ASM_BSS_SECTION);
    DIRECTIVE(".align 8");
    for (size_t i = 0; i < global_symbols->n_symbols; i++)
    {
        symbol_t *symbol = global_symbols->symbols[i];
        if (symbol->type == SYMBOL_GLOBAL_VAR)
        {
            DIRECTIVE(".%s: \t.zero 8", symbol->name);
        }
        else if (symbol->type == SYMBOL_GLOBAL_ARRAY)
        {
            if (symbol->node->children[1]->type != NUMBER_DATA)
            {
                fprintf(stderr, "error: length of array '%s' is not compile time known", symbol->name);
                exit(EXIT_FAILURE);
            }
            int64_t length = *(int64_t *)symbol->node->children[1]->data;
            DIRECTIVE(".%s: \t.zero %ld", symbol->name, length * 8);
        }
    }
}

/* Global variable used to make the functon currently being generated accessible from anywhere */
static symbol_t *current_function;

/* Prints the entry point. preamble, statements and epilouge of the given function */
static void generate_function(symbol_t *function)
{
    LABEL(".%s", function->name);
    current_function = function;

    PUSHQ(RBP);
    MOVQ(RSP, RBP);

    // Up to 6 prameters have been passed in registers. Place them on the stack instead
    for (size_t i = 0; i < FUNC_PARAM_COUNT(function) && i < NUM_REGISTER_PARAMS; i++)
        PUSHQ(REGISTER_PARAMS[i]);

    // Now, for each local variable, push 8-byte 0 values to the stack
    for (size_t i = 0; i < function->function_symtable->n_symbols; i++)
        if (function->function_symtable->symbols[i]->type == SYMBOL_LOCAL_VAR)
            PUSHQ("$0");

    generate_statement(function->node->children[2]);

    // In case the function didn't return, return 0 here
    MOVQ("$0", RAX);
    // leaveq is written out manually, to increase clarity of what happens
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static void generate_function_call(node_t *call)
{
    symbol_t *symbol = call->children[0]->symbol;
    if (symbol->type != SYMBOL_FUNCTION)
    {
        fprintf(stderr, "error: '%s' is not a function\n", symbol->name);
        exit(EXIT_FAILURE);
    }

    node_t *argument_list = call->children[1];

    int parameter_count = FUNC_PARAM_COUNT(symbol);
    if (parameter_count != argument_list->n_children)
    {
        fprintf(stderr, "error: function '%s' expects '%d' arguments, but '%ld' were given\n",
                symbol->name, parameter_count, argument_list->n_children);
        exit(EXIT_FAILURE);
    }

    // We evaluate all parameters from right to left, pushing them to the stack
    for (int i = parameter_count - 1; i >= 0; i--)
    {
        generate_expression(argument_list->children[i]);
        PUSHQ(RAX);
    }

    // Up to 6 parameters should be passed through registers instead. Pop them off the stack
    for (size_t i = 0; i < parameter_count && i < NUM_REGISTER_PARAMS; i++)
        POPQ(REGISTER_PARAMS[i]);

    EMIT("call .%s", symbol->name);

    // Now pop away any stack passed parameters still left on the stack, by moving %rsp upwards
    if (parameter_count > NUM_REGISTER_PARAMS)
        EMIT("addq $%d, %s", (parameter_count - NUM_REGISTER_PARAMS) * 8, RSP);
}

/* Returns a string for accessing the quadword referenced by node */
static const char *generate_variable_access(node_t *node)
{
    static char result[100];

    assert(node->type == IDENTIFIER_DATA);

    symbol_t *symbol = node->symbol;
    switch (symbol->type)
    {
    case SYMBOL_GLOBAL_VAR:
        snprintf(result, sizeof(result), ".%s(%s)", symbol->name, RIP);
        return result;
    case SYMBOL_LOCAL_VAR:
    {
        // If we have more than 6 parameters, subtract away the hole in the sequence numbers
        int call_frame_offset = symbol->sequence_number;
        if (FUNC_PARAM_COUNT(current_function) > NUM_REGISTER_PARAMS)
            call_frame_offset -= FUNC_PARAM_COUNT(current_function) - NUM_REGISTER_PARAMS;
        // The stack grows down, in multiples of 8, and sequence number 0 corresponds to -8
        call_frame_offset = (-call_frame_offset - 1) * 8;

        snprintf(result, sizeof(result), "%d(%s)", call_frame_offset, RBP);
        return result;
    }
    case SYMBOL_PARAMETER:
    {
        int call_frame_offset;
        // Handle the first 6 parameters differently
        if (symbol->sequence_number < NUM_REGISTER_PARAMS)
            // Move along down the stack, with parameter 0 at position -8(%rbp)
            call_frame_offset = (-symbol->sequence_number - 1) * 8;
        else
            // Parameter 6 is at 16(%rbp), with further parameters moving up from there
            call_frame_offset = 16 + (symbol->sequence_number - NUM_REGISTER_PARAMS) * 8;

        snprintf(result, sizeof(result), "%d(%s)", call_frame_offset, RBP);
        return result;
    }
    case SYMBOL_FUNCTION:
        fprintf(stderr, "error: symbol '%s' is a function, not a variable\n", symbol->name);
        exit(EXIT_FAILURE);
    case SYMBOL_GLOBAL_ARRAY:
        fprintf(stderr, "error: symbol '%s' is an array, not a variable\n", symbol->name);
        exit(EXIT_FAILURE);
    default:
        assert(false && "Unknown variable symbol type");
    }
}

/* Takes in an ARRAY_INDEXING node, such as array[x]
 * The function emits code to evaluate x, which may clobber all registers.
 * Once x is evaluated, the address of array[x] is calculated, and stored in the RCX register.
 * The return value is the string "(%rcx)", the assembly for using RCX as an address.
 */
static const char *generate_array_access(node_t *node)
{
    assert(node->type == ARRAY_INDEXING);

    symbol_t *symbol = node->children[0]->symbol;
    if (symbol->type != SYMBOL_GLOBAL_ARRAY)
    {
        fprintf(stderr, "error: symbol '%s' is not an array\n", symbol->name);
        exit(EXIT_FAILURE);
    }

    // Calculate the index of the array into %rax
    generate_expression(node->children[1]);

    // Place the base of the array into %rcx
    EMIT("leaq .%s(%s), %s", symbol->name, RIP, RCX);

    // Place the exact position of the element we wish to access, into %rcx
    EMIT("leaq (%s, %s, 8), %s", RCX, RAX, RCX);

    // Now, the address of the element is stored at %rcx, so just use MEM() to reference it
    return MEM(RCX);
}

/* Generates code to evaluate the expression, and place the result in %rax */
static void generate_expression(node_t *expression)
{
    switch (expression->type)
    {
    case NUMBER_DATA:
        // Simply place the number into %rax
        EMIT("movq $%ld, %s", *(int64_t *)expression->data, RAX);
        break;
    case IDENTIFIER_DATA:
        // Load the variable, and put the result in RAX
        MOVQ(generate_variable_access(expression), RAX);
        break;
    case ARRAY_INDEXING:
        // Load the value pointed to by array[idx], and put the result in RAX
        MOVQ(generate_array_access(expression), RAX);
        break;
    case EXPRESSION:
    {
        char *data = expression->data;
        if (strcmp(data, "+") == 0)
        {
            generate_expression(expression->children[0]);
            PUSHQ(RAX);
            generate_expression(expression->children[1]);
            POPQ(RCX);
            ADDQ(RCX, RAX);
        }
        else if (strcmp(data, "-") == 0)
        {
            if (expression->n_children == 1)
            {
                // Unary minus
                generate_expression(expression->children[0]);
                NEGQ(RAX);
            }
            else
            {
                // Binary minus. Evaluate RHS first, to get the result in RAX easier
                generate_expression(expression->children[1]);
                PUSHQ(RAX);
                generate_expression(expression->children[0]);
                POPQ(RCX);
                SUBQ(RCX, RAX);
            }
        }
        else if (strcmp(data, "*") == 0)
        {
            // Multiplication does not need to do sign extend
            generate_expression(expression->children[0]);
            PUSHQ(RAX);
            generate_expression(expression->children[1]);
            POPQ(RCX);
            IMULQ(RCX, RAX);
        }
        else if (strcmp(data, "/") == 0)
        {
            generate_expression(expression->children[1]);
            PUSHQ(RAX);
            generate_expression(expression->children[0]);
            CQO; // Sign extend RAX -> RDX:RAX
            POPQ(RCX);
            IDIVQ(RCX); // Didivde RDX:RAX by RCX, placing the result in RAX
        }
        else if (strcmp(data, "<<") == 0)
        {
            // Evaluate the shift amount first, and push it to stack
            generate_expression(expression->children[1]);
            PUSHQ(RAX);
            generate_expression(expression->children[0]);
            POPQ(RCX);    // Pop the shift amount
            SAL(CL, RAX); // RAX = RAX<<CL
        }
        else if (strcmp(data, ">>") == 0)
        {
            // Evaluate the shift amount first, and push it to stack
            generate_expression(expression->children[1]);
            PUSHQ(RAX);
            generate_expression(expression->children[0]);
            POPQ(RCX);    // Pop the shift amount
            SAR(CL, RAX); // RAX = RAX>>CL
        }
        else
            assert(false && "Unknown expression operation");
        break;
    }
    case FUNCTION_CALL:
        generate_function_call(expression);
        break;
    default:
        assert(false && "Unknown expression type");
    }
}

static void generate_assignment_statement(node_t *statement)
{
    node_t *dest = statement->children[0];
    node_t *expression = statement->children[1];

    // First the right hand side of the assignment is evaluated
    generate_expression(expression);

    if (dest->type == IDENTIFIER_DATA)
        // Store rax into the memory location corresponding to the variable
        MOVQ(RAX, generate_variable_access(dest));
    else
    {
        // Store rax until the final address of the array element is found,
        // since array index calculation can potentially modify all registers
        PUSHQ(RAX);
        const char *dest_mem = generate_array_access(dest);
        POPQ(RAX);
        MOVQ(RAX, dest_mem);
    }
}

static void generate_print_statement(node_t *statement)
{
    node_t *print_items = statement->children[0];
    for (size_t i = 0; i < print_items->n_children; i++)
    {
        node_t *item = print_items->children[i];
        if (item->type == STRING_LIST_REFERENCE)
        {
            EMIT("leaq strout(%s), %s", RIP, RDI);
            EMIT("leaq string%zu(%s), %s", (size_t)item->data, RIP, RSI);
        }
        else
        {
            generate_expression(item);
            MOVQ(RAX, RSI);
            EMIT("leaq intout(%s), %s", RIP, RDI);
        }
        EMIT("call safe_printf");
    }

    MOVQ("$'\\n'", RDI);
    EMIT("call putchar");
}

static void generate_return_statement(node_t *statement)
{
    generate_expression(statement->children[0]);
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static void generate_relation(node_t *relation)
{

    if (relation->n_children < 2)
        return; // No relation to check

    node_t *lhs = relation->children[0]; // assume lhs is the first child
    node_t *rhs = relation->children[1]; // assume rhs is the second child

    generate_expression(lhs);
    PUSHQ(RAX);
    generate_expression(rhs);
    POPQ(RCX);
    CMPQ(RAX, RCX);
}

static char *get_jump_operator(char *operator)
{
    char *out_statement = "";
    if (strcmp(operator, "=") == 0)
    {
        out_statement = "jne";
    }
    else if (strcmp(operator, "!=") == 0)
    {
        out_statement = "je";
    }
    else if (strcmp(operator, "<") == 0)
    {
        out_statement = "jge";
    }
    else if (strcmp(operator, ">") == 0)
    {
        out_statement = "jle";
    }
    return out_statement;
}

static void generate_if_statement(node_t *statement)
{

    if_label_counter++;
    size_t local_if_label_counter = if_label_counter;

    size_t n_children = statement->n_children;

    if (n_children < 2)
        return; // No statement to generate

    node_t *relation = statement->children[0];
    node_t *then_statement = statement->children[1];

    // generate label for the if statement block
    LABEL("IF%zu", local_if_label_counter);
    generate_relation(relation);

    char *out_statement = "";
    if (n_children == 3)
        out_statement = "ELSE";
    else if (n_children == 2)
        out_statement = "END_IF";

    char *operator= get_jump_operator((char *)relation->data);
    if (strcmp(operator, "") == 0)
        assert(false && "Unknown jump operator type");

    DIRECTIVE("%s %s%zu", operator, out_statement, local_if_label_counter);

    LABEL("THEN%zu", local_if_label_counter);

    generate_statement(then_statement);

    DIRECTIVE("jmp END_IF%zu ", local_if_label_counter);

    if (n_children == 3)
    {

        node_t *else_statement = statement->children[2];
        LABEL("ELSE%zu", local_if_label_counter);

        generate_statement(else_statement);
    }
    LABEL("END_IF%zu", local_if_label_counter);
}

static void generate_while_statement(node_t *statement)
{

    size_t n_children = statement->n_children;

    if (n_children < 2)
    {
        printf("Error: While statement has less than 2 children\n");
        return; // Assumes a while statement has 2 children, a relation and a block
    }
    while_label_counter++;
    size_t local_while_label_counter = while_label_counter;
    break_loop_id = local_while_label_counter;

    node_t *relation = statement->children[0];

    LABEL("LOOP%zu", local_while_label_counter);

    generate_relation(relation);

    char *operator= get_jump_operator((char *)relation->data);
    if (strcmp(operator, "") == 0)
        assert(false && "Unknown jump operator type");

    DIRECTIVE("%s END_LOOP%zu", operator, local_while_label_counter);

    generate_statement(statement->children[1]);
    DIRECTIVE("jmp LOOP%zu", local_while_label_counter);

    LABEL("END_LOOP%zu", local_while_label_counter);
    break_loop_id--;
}

// Leaves the currently innermost while loop using its ENDWHILE label
static void generate_break_statement()
{

    if (break_loop_id == 0)
    {
        assert(false && "outside of loop, cannot break");
    }

    DIRECTIVE("jmp END_LOOP%zu", break_loop_id);
}

/* Recursively generate the given statement node, and all sub-statements. */
static void generate_statement(node_t *node)
{
    switch (node->type)
    {
    case BLOCK:
    {
        // All handling of pushing and popping scopes has already been done
        // Just generate the statements that make up the statement body, one by one
        node_t *statement_list = node->children[node->n_children - 1];
        for (size_t i = 0; i < statement_list->n_children; i++)
            generate_statement(statement_list->children[i]);
        break;
    }
    case ASSIGNMENT_STATEMENT:
        generate_assignment_statement(node);
        break;
    case PRINT_STATEMENT:
        generate_print_statement(node);
        break;
    case RETURN_STATEMENT:
        generate_return_statement(node);
        break;
    case IF_STATEMENT:
        generate_if_statement(node);
        break;
    case WHILE_STATEMENT:
        generate_while_statement(node);
        break;
    case BREAK_STATEMENT:
        generate_break_statement();
        break;
    case FUNCTION_CALL:
        generate_function_call(node);
        break;
    default:
        assert(false && "Unknown statement type");
    }
}

static void generate_safe_printf(void)
{
    LABEL("safe_printf");

    PUSHQ(RBP);
    MOVQ(RSP, RBP);
    // This is a bitmask that abuses how negative numbers work, to clear the last 4 bits
    // A stack pointer that is not 16-byte aligned, will be moved down to a 16-byte boundary
    ANDQ("$-16", RSP);
    EMIT("call printf");
    // Cleanup the stack back to how it was
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static void generate_main(symbol_t *first)
{
    // Make the globally available main function
    LABEL("main");

    // Save old base pointer, and set new base pointer
    PUSHQ(RBP);
    MOVQ(RSP, RBP);

    // Which registers argc and argv are passed in
    const char *argc = RDI;
    const char *argv = RSI;

    const size_t expected_args = FUNC_PARAM_COUNT(first);

    SUBQ("$1", argc); // argc counts the name of the binary, so subtract that
    EMIT("cmpq $%ld, %s", expected_args, argc);
    JNE("ABORT"); // If the provdied number of arguments is not equal, go to the abort label

    if (expected_args == 0)
        goto skip_args; // No need to parse argv

    // Now we emit a loop to parse all parameters, and push them to the stack,
    // in right-to-left order

    // First move the argv pointer to the vert rightmost parameter
    EMIT("addq $%ld, %s", expected_args * 8, argv);

    // We use rcx as a counter, starting at the number of arguments
    MOVQ(argc, RCX);
    LABEL("PARSE_ARGV"); // A loop to parse all parameters
    PUSHQ(argv);         // push registers to caller save them
    PUSHQ(RCX);

    // Now call strtol to parse the argument
    EMIT("movq (%s), %s", argv, RDI); // 1st argument, the char *
    MOVQ("$0", RSI);                  // 2nd argument, a null pointer
    MOVQ("$10", RDX);                 // 3rd argument, we want base 10
    EMIT("call strtol");

    // Restore caller saved registers
    POPQ(RCX);
    POPQ(argv);
    PUSHQ(RAX); // Store the parsed argument on the stack

    SUBQ("$8", argv);        // Point to the previous char*
    EMIT("loop PARSE_ARGV"); // Loop uses RCX as a counter automatically

    // Now, pop up to 6 arguments into registers instead of stack
    for (size_t i = 0; i < expected_args && i < NUM_REGISTER_PARAMS; i++)
        POPQ(REGISTER_PARAMS[i]);

skip_args:

    EMIT("call .%s", first->name);
    MOVQ(RAX, RDI);    // Move the return value of the function into RDI
    EMIT("call exit"); // Exit with the return value as exit code

    LABEL("ABORT"); // In case of incorrect number of arguments
    EMIT("leaq errout(%s), %s", RIP, RDI);
    EMIT("call puts"); // print the errout string
    MOVQ("$1", RDI);
    EMIT("call exit"); // Exit with return code 1

    generate_safe_printf();

    // Declares global symbols we use or emit, such as main, printf and putchar
    DIRECTIVE("%s", ASM_DECLARE_SYMBOLS);
}
