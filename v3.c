/*
* ============================================================================
* 隐私信息管理系统 v3.0 【数据结构期末大作业 高分完整版】
* 技术点：单向链表 | 文件持久化 | 软删除 | 权限控制 | 数据脱敏 | 多字段搜索
* 拓展功能：冒泡+快速排序 | 分页展示 | 数据合法性校验 | 日志管理 | 安全处理
* 新增功能：批量生成测试数据（管理员专属）
* ============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// ====================== 全局宏定义 ======================
#define MAX_NAME_LEN 32
#define MAX_ID_LEN 32
#define MAX_PHONE_LEN 32
#define MAX_TYPE_LEN 32
#define MAX_REMARK_LEN 64
#define MAX_USERNAME_LEN 16
#define MAX_PWD_LEN 16
#define DATA_FILE "records.dat"
#define AUDIT_FILE "audit.log"
#define MASK_KEEP_FRONT 3
#define MASK_KEEP_BACK 2
#define LOGIN_MAX_TRY 3
#define PAGE_SIZE 10
#define MAX_RECORDS 2000   // 快速排序辅助数组上限（已扩容以支持批量生成）

// ====================== 结构体定义 ======================
typedef struct {
	char username[MAX_USERNAME_LEN];
	char password[MAX_PWD_LEN];
	int is_admin;
} User;

typedef struct PrivacyRecordNode {
	int id;
	char owner_name[MAX_NAME_LEN];
	char id_number[MAX_ID_LEN];
	char phone[MAX_PHONE_LEN];
	char info_type[MAX_TYPE_LEN];
	char remark[MAX_REMARK_LEN];
	int is_deleted; // 0=正常 1=软删除
	struct PrivacyRecordNode *next;
} PrivacyRecordNode;

static User g_current_user;
static PrivacyRecordNode *g_record_head = NULL;

// ====================== 函数声明 ======================
static void trim_newline(char *s);
static void read_line(char *buf, size_t len);
static void safe_strcpy(char *dst, const char *src, size_t dst_size);
static void mask_string(const char *src, char *dst, size_t dst_len, int keep_front, int keep_back);
static void get_time_str(char *buf, size_t len);
static void log_action(const char *username, const char *action, const char *detail);
static void clear_stdin_line(void);
static int is_all_space(const char *s);

static PrivacyRecordNode* create_node(void);
static void free_list(void);
static void node_to_buf(PrivacyRecordNode *node, char *buf);
static void buf_to_node(char *buf, PrivacyRecordNode *node);
static void load_records(void);
static void save_records(void);
static int get_next_id(void);
static PrivacyRecordNode* find_node_by_id(int target_id);

static void swap_node_data(PrivacyRecordNode *a, PrivacyRecordNode *b);
static void sort_list_by_id_bubble(void);
static void sort_list_by_name_bubble(void);
static void sort_list_by_id_quick(void);
static void sort_list_by_name_quick(void);

static void init_users(User *users, int *count);
static int login(void);

static void add_record(void);
static void list_records_masked(void);
static void list_records_page(void);
static void view_record_detail(void);
static void search_records(void);
static void update_record(void);
static void soft_delete_record(void);
static void list_deleted_records(void);
static void restore_record(void);
static void export_to_text(void);
static void query_audit_log(void);
static void clear_audit_log(void);
static void sort_menu(void);
static void print_complexity_report(void);
static void generate_test_data(void);  // 新增：批量生成测试数据

static void print_main_menu(void);

// ====================== 工具函数 ======================
static void trim_newline(char *s) {
	if (!s) return;
	size_t len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
		s[--len] = '\0';
	}
}

static void read_line(char *buf, size_t len) {
	if (!buf || len == 0) return;
	if (fgets(buf, (int)len, stdin) == NULL) {
		buf[0] = '\0';
		return;
	}
	trim_newline(buf);
}

static void safe_strcpy(char *dst, const char *src, size_t dst_size) {
	if (!dst || !src || dst_size == 0) return;
	strncpy(dst, src, dst_size - 1);
	dst[dst_size - 1] = '\0';
}

static void mask_string(const char *src, char *dst, size_t dst_len, int keep_front, int keep_back) {
	if (!src || !dst || dst_len == 0) return;
	size_t len = strlen(src);
	if ((size_t)keep_front + (size_t)keep_back >= len) {
		safe_strcpy(dst, src, dst_len);
		return;
	}
	size_t i = 0;
	for (; i < (size_t)keep_front && i < len && i < dst_len - 1; i++) dst[i] = src[i];
	for (; i < len - (size_t)keep_back && i < dst_len - 1; i++) dst[i] = '*';
	for (; i < len && i < dst_len - 1; i++) dst[i] = src[i];
	dst[i < dst_len ? i : dst_len - 1] = '\0';
}

static void get_time_str(char *buf, size_t len) {
	if (!buf || len == 0) return;
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	if (tm_info) strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
	else buf[0] = '\0';
}

static void log_action(const char *username, const char *action, const char *detail) {
	FILE *fp = fopen(AUDIT_FILE, "a");
	if (!fp) return;
	char timebuf[32];
	get_time_str(timebuf, sizeof(timebuf));
	fprintf(fp, "[%s] user=%s action=%s detail=%s\n",
		timebuf,
		username ? username : "-",
		action ? action : "-",
		detail ? detail : "-");
	fclose(fp);
}

static void clear_stdin_line(void) {
	int c;
	while ((c = getchar()) != '\n' && c != EOF);
}

static int is_all_space(const char *s) {
	if (!s) return 1;
	while (*s) {
		if (!isspace((unsigned char)*s)) return 0;
		s++;
	}
	return 1;
}

// ====================== 链表核心操作 ======================
static PrivacyRecordNode* create_node(void) {
	PrivacyRecordNode *node = (PrivacyRecordNode*)malloc(sizeof(PrivacyRecordNode));
	if (node) {
		memset(node, 0, sizeof(PrivacyRecordNode));
		node->next = NULL;
	}
	return node;
}

static void free_list(void) {
	PrivacyRecordNode *p = g_record_head;
	while (p) {
		PrivacyRecordNode *temp = p;
		p = p->next;
		free(temp);
	}
	g_record_head = NULL;
}

#define REC_DATA_LEN (sizeof(int) + MAX_NAME_LEN + MAX_ID_LEN + MAX_PHONE_LEN + MAX_TYPE_LEN + MAX_REMARK_LEN + sizeof(int))

static void node_to_buf(PrivacyRecordNode *node, char *buf) {
	int pos = 0;
	memcpy(buf + pos, &node->id, sizeof(int)); pos += sizeof(int);
	memcpy(buf + pos, node->owner_name, MAX_NAME_LEN); pos += MAX_NAME_LEN;
	memcpy(buf + pos, node->id_number, MAX_ID_LEN); pos += MAX_ID_LEN;
	memcpy(buf + pos, node->phone, MAX_PHONE_LEN); pos += MAX_PHONE_LEN;
	memcpy(buf + pos, node->info_type, MAX_TYPE_LEN); pos += MAX_TYPE_LEN;
	memcpy(buf + pos, node->remark, MAX_REMARK_LEN); pos += MAX_REMARK_LEN;
	memcpy(buf + pos, &node->is_deleted, sizeof(int)); pos += sizeof(int);
}

static void buf_to_node(char *buf, PrivacyRecordNode *node) {
	int pos = 0;
	memcpy(&node->id, buf + pos, sizeof(int)); pos += sizeof(int);
	memcpy(node->owner_name, buf + pos, MAX_NAME_LEN); pos += MAX_NAME_LEN;
	memcpy(node->id_number, buf + pos, MAX_ID_LEN); pos += MAX_ID_LEN;
	memcpy(node->phone, buf + pos, MAX_PHONE_LEN); pos += MAX_PHONE_LEN;
	memcpy(node->info_type, buf + pos, MAX_TYPE_LEN); pos += MAX_TYPE_LEN;
	memcpy(node->remark, buf + pos, MAX_REMARK_LEN); pos += MAX_REMARK_LEN;
	memcpy(&node->is_deleted, buf + pos, sizeof(int)); pos += sizeof(int);
	node->next = NULL;
}

static void load_records(void) {
	FILE *fp = fopen(DATA_FILE, "rb");
	char buf[REC_DATA_LEN];
	if (!fp) return;
	free_list();
	while (fread(buf, REC_DATA_LEN, 1, fp) == 1) {
		PrivacyRecordNode *node = create_node();
		if (node) {
			buf_to_node(buf, node);
			if (!g_record_head) g_record_head = node;
			else {
				PrivacyRecordNode *p = g_record_head;
				while (p->next) p = p->next;
				p->next = node;
			}
		}
	}
	fclose(fp);
}

static void save_records(void) {
	FILE *fp = fopen(DATA_FILE, "wb");
	if (!fp) return;
	char buf[REC_DATA_LEN];
	for (PrivacyRecordNode *p = g_record_head; p; p = p->next) {
		node_to_buf(p, buf);
		fwrite(buf, REC_DATA_LEN, 1, fp);
	}
	fclose(fp);
}

static int get_next_id(void) {
	int max_id = 0;
	for (PrivacyRecordNode *p = g_record_head; p; p = p->next) {
		if (p->id > max_id) max_id = p->id;
	}
	return max_id + 1;
}

static PrivacyRecordNode* find_node_by_id(int target_id) {
	for (PrivacyRecordNode *p = g_record_head; p; p = p->next) {
		if (p->id == target_id) return p;
	}
	return NULL;
}

// ====================== 排序模块 ======================
static void swap_node_data(PrivacyRecordNode *a, PrivacyRecordNode *b) {
	int temp_id = a->id; a->id = b->id; b->id = temp_id;
	int temp_del = a->is_deleted; a->is_deleted = b->is_deleted; b->is_deleted = temp_del;
	
	char temp[MAX_REMARK_LEN + 1];
	safe_strcpy(temp, a->owner_name, sizeof(temp));
	safe_strcpy(a->owner_name, b->owner_name, MAX_NAME_LEN);
	safe_strcpy(b->owner_name, temp, MAX_NAME_LEN);
	
	safe_strcpy(temp, a->id_number, sizeof(temp));
	safe_strcpy(a->id_number, b->id_number, MAX_ID_LEN);
	safe_strcpy(b->id_number, temp, MAX_ID_LEN);
	
	safe_strcpy(temp, a->phone, sizeof(temp));
	safe_strcpy(a->phone, b->phone, MAX_PHONE_LEN);
	safe_strcpy(b->phone, temp, MAX_PHONE_LEN);
	
	safe_strcpy(temp, a->info_type, sizeof(temp));
	safe_strcpy(a->info_type, b->info_type, MAX_TYPE_LEN);
	safe_strcpy(b->info_type, temp, MAX_TYPE_LEN);
	
	safe_strcpy(temp, a->remark, sizeof(temp));
	safe_strcpy(a->remark, b->remark, MAX_REMARK_LEN);
	safe_strcpy(b->remark, temp, MAX_REMARK_LEN);
}

static void sort_list_by_id_bubble(void) {
	if (!g_record_head || !g_record_head->next) return;
	int flag;
	do {
		flag = 0;
		for (PrivacyRecordNode *p = g_record_head; p->next; p = p->next) {
			if (p->id > p->next->id) {
				swap_node_data(p, p->next);
				flag = 1;
			}
		}
	} while (flag);
	save_records();
	printf(" 冒泡排序完成（按ID）\n");
	log_action(g_current_user.username, "sort", "bubble by id");
}

static void sort_list_by_name_bubble(void) {
	if (!g_record_head || !g_record_head->next) return;
	int flag;
	do {
		flag = 0;
		for (PrivacyRecordNode *p = g_record_head; p->next; p = p->next) {
			if (strcmp(p->owner_name, p->next->owner_name) > 0) {
				swap_node_data(p, p->next);
				flag = 1;
			}
		}
	} while (flag);
	save_records();
	printf(" 冒泡排序完成（按姓名）\n");
	log_action(g_current_user.username, "sort", "bubble by name");
}

static int quick_sort_partition(PrivacyRecordNode **arr, int low, int high, int by_id) {
	PrivacyRecordNode *pivot = arr[high];
	int i = low - 1;
	for (int j = low; j < high; j++) {
		int should = by_id ? (arr[j]->id <= pivot->id) : (strcmp(arr[j]->owner_name, pivot->owner_name) <= 0);
		if (should) {
			i++;
			PrivacyRecordNode *tmp = arr[i];
			arr[i] = arr[j];
			arr[j] = tmp;
		}
	}
	PrivacyRecordNode *tmp = arr[i + 1];
	arr[i + 1] = arr[high];
	arr[high] = tmp;
	return i + 1;
}

static void quick_sort_recursive(PrivacyRecordNode **arr, int low, int high, int by_id) {
	if (low < high) {
		int pi = quick_sort_partition(arr, low, high, by_id);
		quick_sort_recursive(arr, low, pi - 1, by_id);
		quick_sort_recursive(arr, pi + 1, high, by_id);
	}
}

// 已修复：不丢失已删除记录
static void sort_list_by_id_quick(void) {
	PrivacyRecordNode *normal[MAX_RECORDS], *deleted[MAX_RECORDS];
	int normal_cnt = 0, deleted_cnt = 0;
	
	for (PrivacyRecordNode *p = g_record_head; p; p = p->next) {
		if (p->is_deleted)
			deleted[deleted_cnt++] = p;
		else
			normal[normal_cnt++] = p;
	}
	
	if (normal_cnt > 1) {
		quick_sort_recursive(normal, 0, normal_cnt - 1, 1);
	}
	
	// 重新连接：正常记录在前，已删除记录在后
	for (int i = 0; i < normal_cnt - 1; i++)
		normal[i]->next = normal[i + 1];
	if (normal_cnt > 0)
		normal[normal_cnt - 1]->next = (deleted_cnt > 0 ? deleted[0] : NULL);
	
	for (int i = 0; i < deleted_cnt - 1; i++)
		deleted[i]->next = deleted[i + 1];
	if (deleted_cnt > 0)
		deleted[deleted_cnt - 1]->next = NULL;
	
	g_record_head = (normal_cnt > 0 ? normal[0] : (deleted_cnt > 0 ? deleted[0] : NULL));
	
	save_records();
	printf(" 快速排序完成（按ID，O(n log n)）\n");
	log_action(g_current_user.username, "sort", "quick by id");
}

static void sort_list_by_name_quick(void) {
	PrivacyRecordNode *normal[MAX_RECORDS], *deleted[MAX_RECORDS];
	int normal_cnt = 0, deleted_cnt = 0;
	
	for (PrivacyRecordNode *p = g_record_head; p; p = p->next) {
		if (p->is_deleted)
			deleted[deleted_cnt++] = p;
		else
			normal[normal_cnt++] = p;
	}
	
	if (normal_cnt > 1) {
		quick_sort_recursive(normal, 0, normal_cnt - 1, 0);
	}
	
	for (int i = 0; i < normal_cnt - 1; i++)
		normal[i]->next = normal[i + 1];
	if (normal_cnt > 0)
		normal[normal_cnt - 1]->next = (deleted_cnt > 0 ? deleted[0] : NULL);
	
	for (int i = 0; i < deleted_cnt - 1; i++)
		deleted[i]->next = deleted[i + 1];
	if (deleted_cnt > 0)
		deleted[deleted_cnt - 1]->next = NULL;
	
	g_record_head = (normal_cnt > 0 ? normal[0] : (deleted_cnt > 0 ? deleted[0] : NULL));
	
	save_records();
	printf(" 快速排序完成（按姓名，O(n log n)）\n");
	log_action(g_current_user.username, "sort", "quick by name");
}

// ====================== 业务功能 ======================
static void add_record(void) {
	PrivacyRecordNode *node = create_node();
	char detail[128];
	if (!node) { printf("内存分配失败！\n"); return; }
	
	printf("\n---------- 新增隐私记录 ----------\n");
	node->id = get_next_id();
	node->is_deleted = 0;
	printf(" 自动生成记录ID：%d\n", node->id);
	
	printf("请输入姓名："); read_line(node->owner_name, MAX_NAME_LEN);
	if (is_all_space(node->owner_name)) {
		printf("× 姓名不能为空！\n"); free(node); return;
	}
	
	printf("请输入证件号/账号："); read_line(node->id_number, MAX_ID_LEN);
	printf("请输入联系电话："); read_line(node->phone, MAX_PHONE_LEN);
	printf("请输入信息类型："); read_line(node->info_type, MAX_TYPE_LEN);
	printf("请输入备注（选填）："); read_line(node->remark, MAX_REMARK_LEN);
	
	if (!g_record_head) g_record_head = node;
	else {
		PrivacyRecordNode *p = g_record_head;
		while (p->next) p = p->next;
		p->next = node;
	}
	
	save_records();
	printf("√ 记录新增成功！\n");
	sprintf(detail, "add id=%d name=%s", node->id, node->owner_name);
	log_action(g_current_user.username, "add_record", detail);
}

static void list_records_masked(void) {
	PrivacyRecordNode *p = g_record_head;
	char id_mask[MAX_ID_LEN], phone_mask[MAX_PHONE_LEN];
	int count = 0;
	
	printf("\n---------- 隐私记录列表（脱敏展示） ----------\n");
	printf("%-4s %-10s %-20s %-15s %-10s\n", "ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("------------------------------------------------------------\n");
	
	while (p) {
		if (!p->is_deleted) {
			mask_string(p->id_number, id_mask, sizeof(id_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
			mask_string(p->phone, phone_mask, sizeof(phone_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
			printf("%-4d %-10s %-20s %-15s %-10s\n", p->id, p->owner_name, id_mask, phone_mask, p->info_type);
			count++;
		}
		p = p->next;
	}
	printf(" 当前正常记录总数：%d 条\n", count);
	log_action(g_current_user.username, "list_records", "masked");
}

static void list_records_page(void) {
	PrivacyRecordNode *p = g_record_head;
	char id_mask[MAX_ID_LEN], phone_mask[MAX_PHONE_LEN];
	int total = 0, page, start, end, i = 0;
	
	while (p) { if (!p->is_deleted) total++; p = p->next; }
	if (total == 0) { printf("\n暂无有效记录！\n"); return; }
	
	printf("\n---------- 分页查询（每页%d条） ----------\n", PAGE_SIZE);
	printf("总记录数：%d 条，请输入页码：", total);
	if (scanf("%d", &page) != 1) { printf("输入无效！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	int max_page = (total + PAGE_SIZE - 1) / PAGE_SIZE;
	if (page < 1 || page > max_page) { printf("页码范围 1~%d\n", max_page); return; }
	
	start = (page - 1) * PAGE_SIZE;
	end = page * PAGE_SIZE;
	p = g_record_head; i = 0;
	
	printf("%-4s %-10s %-20s %-15s %-10s\n", "ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("------------------------------------------------------------\n");
	while (p) {
		if (!p->is_deleted) {
			if (i >= start && i < end) {
				mask_string(p->id_number, id_mask, sizeof(id_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
				mask_string(p->phone, phone_mask, sizeof(phone_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
				printf("%-4d %-10s %-20s %-15s %-10s\n", p->id, p->owner_name, id_mask, phone_mask, p->info_type);
			}
			i++;
		}
		p = p->next;
	}
	printf(" 当前第 %d 页 / 共 %d 页\n", page, max_page);
	
	char log_detail[64];
	sprintf(log_detail, "page=%d", page);
	log_action(g_current_user.username, "list_page", log_detail);
}

static void view_record_detail(void) {
	int target_id;
	PrivacyRecordNode *node;
	printf("\n---------- 查看记录明细 ----------\n");
	printf("请输入记录ID：");
	if (scanf("%d", &target_id) != 1) { printf("输入格式错误！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	node = find_node_by_id(target_id);
	if (!node) { printf("未找到该记录！\n"); return; }
	if (node->is_deleted) { printf("该记录已被软删除！\n"); return; }
	
	printf("\n【记录详情】\n");
	printf(" 记录ID：%d\n", node->id);
	printf(" 姓 名：%s\n", node->owner_name);
	printf(" 证件账号：%s\n", node->id_number);
	printf(" 联系电话：%s\n", node->phone);
	printf(" 信息类型：%s\n", node->info_type);
	printf(" 备 注：%s\n", node->remark);
	
	char log_detail[64];
	sprintf(log_detail, "id=%d", target_id);
	log_action(g_current_user.username, "view_detail", log_detail);
}

static void search_records(void) {
	char key[64];
	PrivacyRecordNode *p = g_record_head;
	char id_mask[MAX_ID_LEN], phone_mask[MAX_PHONE_LEN];
	int count = 0;
	
	printf("\n---------- 多字段高级搜索 ----------\n");
	printf("请输入搜索关键词：");
	read_line(key, sizeof(key));
	if (strlen(key) == 0 || is_all_space(key)) { printf("关键词不能为空！\n"); return; }
	
	printf("%-4s %-10s %-20s %-15s %-10s\n", "ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("------------------------------------------------------------\n");
	
	while (p) {
		if (!p->is_deleted && (strstr(p->owner_name, key) || strstr(p->id_number, key) ||
			strstr(p->phone, key) || strstr(p->info_type, key) || strstr(p->remark, key))) {
			mask_string(p->id_number, id_mask, sizeof(id_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
			mask_string(p->phone, phone_mask, sizeof(phone_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
			printf("%-4d %-10s %-20s %-15s %-10s\n", p->id, p->owner_name, id_mask, phone_mask, p->info_type);
			count++;
		}
		p = p->next;
	}
	printf(" 搜索完成，共匹配到 %d 条记录\n", count);
	log_action(g_current_user.username, "search", key);
}

static void update_record(void) {
	int target_id, choice;
	PrivacyRecordNode *node;
	char input[64], detail[80];
	
	printf("\n---------- 修改记录 ----------\n");
	printf("请输入待修改记录ID：");
	if (scanf("%d", &target_id) != 1) { printf("输入错误！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	node = find_node_by_id(target_id);
	if (!node) { printf("未找到该记录！\n"); return; }
	if (node->is_deleted) { printf("该记录已删除，禁止修改！\n"); return; }
	
	printf("当前姓名：%s | 新姓名(回车跳过)：", node->owner_name); read_line(input, sizeof(input));
	if (!is_all_space(input)) safe_strcpy(node->owner_name, input, MAX_NAME_LEN);
	
	printf("当前证件号：%s | 新证件号(回车跳过)：", node->id_number); read_line(input, sizeof(input));
	if (!is_all_space(input)) safe_strcpy(node->id_number, input, MAX_ID_LEN);
	
	printf("当前电话：%s | 新电话(回车跳过)：", node->phone); read_line(input, sizeof(input));
	if (!is_all_space(input)) safe_strcpy(node->phone, input, MAX_PHONE_LEN);
	
	printf("当前类型：%s | 新类型(回车跳过)：", node->info_type); read_line(input, sizeof(input));
	if (!is_all_space(input)) safe_strcpy(node->info_type, input, MAX_TYPE_LEN);
	
	printf("当前备注：%s | 新备注(回车跳过)：", node->remark); read_line(input, sizeof(input));
	if (!is_all_space(input)) safe_strcpy(node->remark, input, MAX_REMARK_LEN);
	
	printf("确认保存修改？(1=是 0=否)：");
	if (scanf("%d", &choice) != 1 || choice != 1) { printf("已取消！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	save_records();
	printf("√ 记录修改成功！\n");
	sprintf(detail, "update id=%d", target_id);
	log_action(g_current_user.username, "update_record", detail);
}

static void soft_delete_record(void) {
	int target_id;
	PrivacyRecordNode *node;
	char detail[80];
	if (!g_current_user.is_admin) { printf("× 权限不足！\n"); return; }
	
	printf("\n---------- 软删除记录 ----------\n");
	printf("请输入记录ID：");
	if (scanf("%d", &target_id) != 1) { printf("输入错误！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	node = find_node_by_id(target_id);
	if (!node) { printf("未找到记录！\n"); return; }
	if (node->is_deleted) { printf("该记录已删除！\n"); return; }
	
	node->is_deleted = 1;
	save_records();
	printf("√ 记录 %d 软删除成功！\n", target_id);
	sprintf(detail, "soft_delete id=%d", target_id);
	log_action(g_current_user.username, "soft_delete", detail);
}

static void list_deleted_records(void) {
	if (!g_current_user.is_admin) { printf("× 权限不足！\n"); return; }
	
	PrivacyRecordNode *p = g_record_head;
	char id_mask[MAX_ID_LEN], phone_mask[MAX_PHONE_LEN];
	int total = 0, page, start, end, i = 0;
	
	while (p) { if (p->is_deleted) total++; p = p->next; }
	if (total == 0) { printf("\n暂无已删除记录！\n"); return; }
	
	printf("\n---------- 已删除记录（分页） ----------\n");
	printf("总记录：%d 条，请输入页码：", total);
	if (scanf("%d", &page) != 1) { printf("输入无效！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	int max_page = (total + PAGE_SIZE - 1) / PAGE_SIZE;
	if (page < 1 || page > max_page) { printf("页码范围 1~%d\n", max_page); return; }
	
	start = (page - 1) * PAGE_SIZE;
	end = page * PAGE_SIZE;
	p = g_record_head; i = 0;
	
	printf("%-4s %-10s %-20s %-15s %-10s\n", "ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("------------------------------------------------------------\n");
	
	while (p) {
		if (p->is_deleted) {
			if (i >= start && i < end) {
				mask_string(p->id_number, id_mask, sizeof(id_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
				mask_string(p->phone, phone_mask, sizeof(phone_mask), MASK_KEEP_FRONT, MASK_KEEP_BACK);
				printf("%-4d %-10s %-20s %-15s %-10s\n", p->id, p->owner_name, id_mask, phone_mask, p->info_type);
			}
			i++;
		}
		p = p->next;
	}
	printf(" 第 %d 页 / 共 %d 页\n", page, max_page);
}

static void restore_record(void) {
	int target_id;
	PrivacyRecordNode *node;
	char detail[80];
	if (!g_current_user.is_admin) { printf("× 权限不足！\n"); return; }
	
	printf("\n---------- 恢复记录 ----------\n");
	printf("请输入记录ID：");
	if (scanf("%d", &target_id) != 1) { printf("输入错误！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	node = find_node_by_id(target_id);
	if (!node) { printf("未找到记录！\n"); return; }
	if (!node->is_deleted) { printf("该记录未被删除！\n"); return; }
	
	node->is_deleted = 0;
	save_records();
	printf("√ 记录 %d 恢复成功！\n", target_id);
	sprintf(detail, "restore id=%d", target_id);
	log_action(g_current_user.username, "restore_record", detail);
}

static void export_to_text(void) {
	char filename[64], detail[80];
	int count = 0;
	
	printf("\n---------- 导出数据 ----------\n");
	printf("请输入导出文件名(如 data.txt)：");
	read_line(filename, sizeof(filename));
	if (is_all_space(filename)) { printf("文件名不能为空！\n"); return; }
	
	FILE *fp = fopen(filename, "w");
	if (!fp) { printf("创建文件失败！\n"); return; }
	
	fprintf(fp, "===== 隐私信息记录导出 =====\n");
	fprintf(fp, "ID\t姓名\t证件账号\t电话\t信息类型\t备注\n");
	
	for (PrivacyRecordNode *p = g_record_head; p; p = p->next) {
		if (!p->is_deleted) {
			fprintf(fp, "%d\t%s\t%s\t%s\t%s\t%s\n", p->id, p->owner_name, p->id_number, p->phone, p->info_type, p->remark);
			count++;
		}
	}
	fclose(fp);
	printf("√ 导出完成！共 %d 条记录 → %s\n", count, filename);
	sprintf(detail, "export file=%s count=%d", filename, count);
	log_action(g_current_user.username, "export", detail);
}

static void query_audit_log(void) {
	FILE *fp;
	char line[256], user_key[32] = {0}, action_key[32] = {0};
	int count = 0;
	
	printf("\n---------- 审计日志查询 ----------\n");
	printf("按用户名筛选(留空=全部)："); read_line(user_key, sizeof(user_key));
	printf("按操作类型筛选(留空=全部)："); read_line(action_key, sizeof(action_key));
	
	fp = fopen(AUDIT_FILE, "r");
	if (!fp) { printf("暂无日志！\n"); return; }
	
	while (fgets(line, sizeof(line), fp)) {
		int ok = 1;
		if (strlen(user_key) && !strstr(line, user_key)) ok = 0;
		if (strlen(action_key) && !strstr(line, action_key)) ok = 0;
		if (ok) { printf("%s", line); count++; }
	}
	fclose(fp);
	printf("共匹配 %d 条日志\n", count);
}

static void clear_audit_log(void) {
	if (!g_current_user.is_admin) { printf("× 权限不足！\n"); return; }
	FILE *fp = fopen(AUDIT_FILE, "w");
	if (fp) fclose(fp);
	printf("√ 审计日志已清空！\n");
	log_action(g_current_user.username, "clear_log", "success");
}

static void print_complexity_report(void) {
	printf("\n╔═══════════════════════════════════════════════════════════╗\n");
	printf("║                    算法复杂度分析报告                    ║\n");
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	printf("║ 冒泡排序：O(n^2)          快速排序：平均 O(n log n)      ║\n");
	printf("║ 链表操作：O(n)           多字段搜索：O(n*m)             ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n");
}

static void sort_menu(void) {
	int op;
	printf("\n╔═══════════════════════════════════════════════════════════╗\n");
	printf("║                    链表排序功能                          ║\n");
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	printf("║ 1.按ID冒泡排序     2.按姓名冒泡排序                     ║\n");
	printf("║ 3.按ID快速排序     4.按姓名快速排序                     ║\n");
	printf("║ 5.查看复杂度报告   0.返回                               ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n");
	printf("请选择：");
	
	if (scanf("%d", &op) != 1) { printf("输入错误！\n"); clear_stdin_line(); return; }
	clear_stdin_line();
	
	switch (op) {
		case 1: sort_list_by_id_bubble(); break;
		case 2: sort_list_by_name_bubble(); break;
		case 3: sort_list_by_id_quick(); break;
		case 4: sort_list_by_name_quick(); break;
		case 5: print_complexity_report(); break;
		case 0: break;
		default: printf("无效选项！\n");
	}
}

// ====================== 新增：批量生成测试数据 ======================
static void generate_test_data(void) {
	if (!g_current_user.is_admin) {
		printf("× 仅管理员可使用此功能！\n");
		return;
	}
	
	int num;
	printf("\n---------- 批量生成测试数据 ----------\n");
	printf("请输入要生成的记录数量（1~2000）：");
	if (scanf("%d", &num) != 1 || num < 1 || num > 2000) {
		printf("× 输入无效，请输入1~2000之间的整数！\n");
		clear_stdin_line();
		return;
	}
	clear_stdin_line();
	
	// 初始化随机种子
	srand((unsigned)time(NULL));
	
	const char *surnames[] = {"张", "李", "王", "刘", "陈", "杨", "赵", "黄", "周", "吴"};
	const char *types[] = {"身份证", "银行卡", "手机号", "邮箱", "社保号"};
	
	printf("正在生成 %d 条测试记录...\n", num);
	
	for (int i = 0; i < num; i++) {
		PrivacyRecordNode *node = create_node();
		if (!node) {
			printf("⚠ 内存分配失败，已生成 %d 条\n", i);
			break;
		}
		
		node->id = get_next_id();
		node->is_deleted = 0;
		
		// 随机姓名：姓氏 + 名字（随机数字后缀）
		safe_strcpy(node->owner_name, surnames[rand() % 10], MAX_NAME_LEN);
		char temp[16];
		sprintf(temp, "%d", rand() % 1000 + 1);
		strcat(node->owner_name, temp);
		
		// 随机证件号（模拟18位身份证格式）
		sprintf(node->id_number, "1101011990%04d%04d", rand() % 10000, rand() % 10000);
		
		// 随机电话（11位手机号）
		sprintf(node->phone, "138%08d", rand() % 100000000);
		
		// 随机信息类型
		safe_strcpy(node->info_type, types[rand() % 5], MAX_TYPE_LEN);
		
		// 备注
		sprintf(node->remark, "测试数据 %d", i + 1);
		
		// 插入链表尾部
		if (!g_record_head) {
			g_record_head = node;
		} else {
			PrivacyRecordNode *p = g_record_head;
			while (p->next) p = p->next;
			p->next = node;
		}
	}
	
	save_records();
	printf("√ 成功生成 %d 条测试记录！\n", num);
	log_action(g_current_user.username, "generate_test", "batch");
}

// ====================== 主菜单 ======================
static void print_main_menu(void) {
	printf("\n╔═══════════════════════════════════════════════════════════╗\n");
	printf("║           隐私信息管理系统 v3.0           		    ║\n");
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	printf("║ 当前用户：%-10s（%-8s）                          ║\n",
		g_current_user.username, g_current_user.is_admin ? "管理员" : "普通用户");
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	printf("║ 1.新增  2.脱敏列表  3.分页  4.详情  5.搜索  6.修改     ║\n");
	printf("║ 7.删除  8.已删记录  9.恢复 10.导出 11.日志 12.清日志    ║\n");
	printf("║ 13.排序 14.生成测试数据  0.退出                         ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n");
	printf("请输入选项：");
}

// ====================== 主函数 ======================
int main(void) {
	int choice;
	int running = 1;
	
	if (!login()) return 0;
	
	while (running) {
		print_main_menu();
		if (scanf("%d", &choice) != 1) {
			printf("输入必须为数字！\n");
			clear_stdin_line();
			continue;
		}
		clear_stdin_line();
		
		switch (choice) {
			case 1: add_record(); break;
			case 2: list_records_masked(); break;
			case 3: list_records_page(); break;
			case 4: view_record_detail(); break;
			case 5: search_records(); break;
			case 6: update_record(); break;
			case 7: soft_delete_record(); break;
			case 8: list_deleted_records(); break;
			case 9: restore_record(); break;
			case 10: export_to_text(); break;
			case 11: query_audit_log(); break;
			case 12: clear_audit_log(); break;
			case 13: sort_menu(); break;
			case 14: generate_test_data(); break;  // 新增功能
		case 0:
			save_records();
			printf("√ 数据已保存，系统正常退出！\n");
			log_action(g_current_user.username, "logout", "normal");
			running = 0;
			break;
		default:
			printf("× 无效选项！\n");
		}
	}
	
	free_list();
	return 0;
}

// ====================== 登录模块 ======================
static void init_users(User *users, int *count) {
	safe_strcpy(users[0].username, "admin", MAX_USERNAME_LEN);
	safe_strcpy(users[0].password, "admin123", MAX_PWD_LEN);
	users[0].is_admin = 1;
	
	safe_strcpy(users[1].username, "user", MAX_USERNAME_LEN);
	safe_strcpy(users[1].password, "user123", MAX_PWD_LEN);
	users[1].is_admin = 0;
	
	*count = 2;
}

static int login(void) {
	User users[4];
	int user_count, i, try_times = 0;
	char u[32] = {0}, p[32] = {0};
	
	init_users(users, &user_count);
	printf("╔════════════════════════════════════════╗\n");
	printf("║     隐私信息管理系统 v3.0 登录界面     ║\n");
	printf("╚════════════════════════════════════════╝\n");
	
	while (try_times < LOGIN_MAX_TRY) {
		printf("用户名："); read_line(u, sizeof(u));
		printf("密 码："); read_line(p, sizeof(p));
		
		for (i = 0; i < user_count; i++) {
			if (strcmp(u, users[i].username) == 0 && strcmp(p, users[i].password) == 0) {
				g_current_user = users[i];
				printf("\n√ 登录成功！欢迎您，%s（%s）\n",
					g_current_user.username,
					g_current_user.is_admin ? "管理员" : "普通用户");
				log_action(g_current_user.username, "login", "success");
				load_records();
				return 1;
			}
		}
		printf("× 账号或密码错误！剩余尝试次数：%d\n\n", LOGIN_MAX_TRY - try_times - 1);
		try_times++;
	}
	log_action(u, "login", "failed");
	printf("尝试次数过多，程序退出！\n");
	return 0;
}
