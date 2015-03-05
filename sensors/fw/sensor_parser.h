
#ifndef SENSOR_PARSER_CONFIG_H
#define SENSOR_PARSER_CONFIG_H

/*return of interrupt ack*/
#define IRQ_NONE	 	0
#define IRQ_HANDLED      	1

#define SENSOR_MAX_IFS		40
#define NON_INITIAL_IFELSE	0xff
#define ARRAY_SIZE(a)		(int)(sizeof(a)/sizeof(a[0]))

/*
*  intermediate operand
*/
struct im_operand {
	enum { IM_GLOBAL = 0, IM_LOCAL, IM_REGBUF,
		IM_IMM, IM_BEFORE,
		/*only for writereg*/
		IM_REG,
	} type;

	union {
		int immediate;
		/*index of local or global variables*/
		int index;
		struct im_reg {
			__u8 addr;
			__u8 len;
			__u8 flag;
		} reg;
		int ms;
	} data;
};

enum im_op_type { IM_IF = 0,
	/*start of if true*/
	IM_IF_START,
	IM_ELSE, IM_ENDIF,
	IM_SLEEP,
	IM_RETURN,
	/*=*/
	IM_ASSIGN,
	IM_LOGIC_EQ, IM_LOGIC_NEQ, IM_LOGIC_GREATER, IM_LOGIC_LESS,
	IM_LOGIC_GE, IM_LOGIC_LE, IM_LOGIC_AND, IM_LOGIC_OR,
	IM_ARI_ADD, IM_ARI_SUB, IM_ARI_MUL, IM_ARI_DIV, IM_ARI_MOD,
	IM_BIT_OR, IM_BIT_AND, IM_BIT_LSL, IM_BIT_LSR, IM_BIT_NOR,
	IM_ENDIAN_BE16, IM_ENDIAN_BE16_UN, IM_ENDIAN_BE24, IM_ENDIAN_BE32,
	IM_ENDIAN_LE16, IM_ENDIAN_LE16_UN, IM_ENDIAN_LE24, IM_ENDIAN_LE32,
	IM_MIN, IM_MAX,
	IM_LEFT_BRACE, IM_RIGHT_BRACE,
	IM_RESERVE,
};

struct im_action {
	enum im_op_type type;
	struct im_operand operand1;
	struct im_operand operand2;
};

/*two stacks to parse experession*/
#define IM_STACK_MAX_SIZE 		0x80
struct im_operand_stack {
	struct im_operand data[IM_STACK_MAX_SIZE];
	int top;
};

struct im_op_stack {
	enum im_op_type data[IM_STACK_MAX_SIZE];
	int top;
};

/*check long op firstly, then short*/
struct im_op_attr {
	char *op;
	int op_size;
	int op_num;
	int priority;
	enum im_op_type type;
	enum data_op ll_type;
};

struct sensor_parser {
	struct sensor_config *config;
	/*actions which already be translated*/
	int ll_action_num;

	struct im_operand_stack oper_stack;
	struct im_op_stack op_stack;

#define CHARS_PER_LINE			0x80
#define IM_ACTION_MAX_NUM		0x100
	char high_actions[IM_ACTION_MAX_NUM][CHARS_PER_LINE];
	struct im_action im_actions[IM_ACTION_MAX_NUM];
	struct lowlevel_action ll_actions[IM_ACTION_MAX_NUM];

	/*how many valid actions*/
	int high_num;
	int im_num;
	int ll_num;

	/*index of im/ll_actions to be translate, used when do trans*/
	int im_top;
	int ll_top;

	/*other basic infos*/
	int sensor_regs;
	int id_reg_flag;
	int test_reg_addr;
};

static int dump(void);
static int parser(void);

static int sensor_xmlnode_sort(xmlNodePtr node,
			xmlNodePtr *NodeBuf, int *cnt_buf);
static int sensor_parser_sensors(xmlNodePtr node, int num,
			struct sensor_config *configs, int *size);

static int sensor_config_init(xmlNodePtr node,
			struct sensor_config *config);
static int sensor_config_verify(struct sensor_config *config);
static int sensor_config_init_basic(struct sensor_parser *parser,
				xmlNodePtr node);
static int sensor_config_init_odr(struct sensor_parser *parser,
				xmlNodePtr node);
static int sensor_config_init_range(struct sensor_parser *parser,
				xmlNodePtr node);
static int sensor_config_init_sys(struct sensor_parser *parser,
				xmlNodePtr node);
static int sensor_config_init_actions(struct sensor_parser *parser,
				xmlNodePtr node);

static int parser_odr_action(struct sensor_parser *parser,
		char *high_actions, struct odr *odr);
static int parser_range_action(struct sensor_parser *parser,
		char *high_actions, struct range_setting *range);
static int parser_sys_action(struct sensor_parser *parser,
	char *high_actions, struct sysfs_entry *sys, int isshow);
static int parser_sensor_action(struct sensor_parser *parser,
		char *high_actions, enum sensor_action ll_type);

static int sensor_action_str_format(struct sensor_parser *parser,
				char *input);
static int trans_ll_oper(struct im_operand *im_oper,
				struct operand *ll_oper);
static int sensor_trans_ll(struct sensor_parser *parser,
				struct im_action *im_action);
static int sensor_im2ll_actions(struct sensor_parser *parser);

static int sensor_parser_preprocess(enum im_op_type type,
				int op1, int op2, int *output);
static int sensor_optimize_im_actions(struct sensor_parser *parser);

static int sensor_tran_im_ops(struct sensor_parser *parser,
				enum im_op_type type);
static int sensor_high2im_op(struct sensor_parser *parser,
				char *str, int *len);
static int sensor_high2im_operand(struct sensor_parser *parser,
				char *str, int *len);
static int sensor_tran_last_im_op(struct sensor_parser *parser);
static int sensor_high2im_single(struct sensor_parser *parser,
				char *high_action, int *match);
static int sensor_high2im_multi(struct sensor_parser *parser,
				char *high_action, int *match);
static int sensor_high2im_actions(struct sensor_parser *parser);

static void dump_ll_action(struct lowlevel_action *actions,
				int num, int level);
static void dump_im_actions(struct sensor_parser *parser);
static void dump_ll_actions(struct sensor_parser *parser);

static void dump_sensor_config(struct sensor_config *config);

#endif
