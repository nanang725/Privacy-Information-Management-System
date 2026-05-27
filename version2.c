/*
* ============================================================================
*  隐私信息管理系统（期末课程项目示例） - 纯 C 单文件版本（C89/C90 兼容）
* ============================================================================
* 功能概览（v2）：
*   - 登录（内置 admin/user 两个账号）
*   - 新增、列表（脱敏）、查看明细、按姓名搜索、修改
*   - 软删除（仅管理员）、恢复（仅管理员）
*   - 导出、审计日志查询
*   - 数据持久化：records.dat
*   - 审计日志：audit.log
*
* 小熊猫/Dev-C++ 使用：
*   1) 新建 .c 文件，粘贴本代码
*   2) 确保按 C 编译（不是 C++）
*   3) 编译运行
*
* 账号：
*   - admin / admin123（管理员）
*   - user  / user123  （普通用户）
* ============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NAME_LEN   32
#define MAX_ID_LEN     32
#define MAX_PHONE_LEN  32
#define MAX_TYPE_LEN   32
#define DATA_FILE      "records.dat"
#define AUDIT_FILE     "audit.log"

typedef int elem_t;

typedef struct {
	char username[16];
	char password[16];
	int  is_admin;
} User;

typedef struct {
	int  id;
	char owner_name[MAX_NAME_LEN];
	char id_number[MAX_ID_LEN];
	char phone[MAX_PHONE_LEN];
	char info_type[MAX_TYPE_LEN];
	char remark[64];
	int  is_deleted; /* 0=有效, 1=软删除 */
} PrivacyRecord;

static User g_current_user;

/* ----------------------------- 小工具 ----------------------------- */

static void trim_newline(char *s)
{
	size_t len;
	if (s == NULL) return;
	len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
		s[len - 1] = '\0';
		len--;
	}
}

static void read_line(char *buf, size_t len)
{
	if (buf == NULL || len == 0) return;
	if (fgets(buf, (int)len, stdin) == NULL) {
		buf[0] = '\0';
		return;
	}
	trim_newline(buf);
}

static void mask_string(const char *src, char *dst, size_t dst_len,
	int keep_front, int keep_back)
{
	size_t len, i;
	if (src == NULL || dst == NULL || dst_len == 0) return;
	
	len = strlen(src);
	if ((size_t)keep_front + (size_t)keep_back >= len) {
		strncpy(dst, src, dst_len - 1);
		dst[dst_len - 1] = '\0';
		return;
	}
	if (dst_len <= len + 1) {
		strncpy(dst, src, dst_len - 1);
		dst[dst_len - 1] = '\0';
		return;
	}
	
	for (i = 0; i < (size_t)keep_front && i < len; i++) {
		dst[i] = src[i];
	}
	for (; i < len - (size_t)keep_back; i++) {
		dst[i] = '*';
	}
	for (; i < len; i++) {
		dst[i] = src[i];
	}
	dst[len] = '\0';
}

static void get_time_str(char *buf, size_t len)
{
	time_t t;
	struct tm *tm_info;
	if (buf == NULL || len == 0) return;
	t = time(NULL);
	tm_info = localtime(&t);
	if (tm_info == NULL) {
		buf[0] = '\0';
		return;
	}
	strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

static void log_action(const char *username, const char *action, const char *detail)
{
	FILE *fp;
	char timebuf[32];
	
	get_time_str(timebuf, sizeof(timebuf));
	fp = fopen(AUDIT_FILE, "a");
	if (fp == NULL) return;
	
	fprintf(fp, "[%s] user=%s action=%s detail=%s\n",
		timebuf,
		(username != NULL) ? username : "-",
		(action != NULL) ? action : "-",
		(detail != NULL) ? detail : "-");
	fclose(fp);
}

static void clear_stdin_line(void)
{
	int c;
	do {
		c = getchar();
	} while (c != '\n' && c != EOF);
}

/* ----------------------------- 登录 ----------------------------- */

static void init_users(User *users, int *count)
{
	strcpy(users[0].username, "admin");
	strcpy(users[0].password, "admin123");
	users[0].is_admin = 1;
	
	strcpy(users[1].username, "user");
	strcpy(users[1].password, "user123");
	users[1].is_admin = 0;
	
	*count = 2;
}

static int login(void)
{
	User users[4];
	int user_count;
	int i;
	int try_times;
	char u[32];
	char p[32];
	
	init_users(users, &user_count);
	try_times = 0;
	
	printf("===== 隐私信息管理系统 - 登录 =====\n");
	while (try_times < 3) {
		printf("用户名: ");
		read_line(u, sizeof(u));
		printf("密码: ");
		read_line(p, sizeof(p));
		
		for (i = 0; i < user_count; i++) {
			if (strcmp(u, users[i].username) == 0 &&
				strcmp(p, users[i].password) == 0) {
				g_current_user = users[i];
				printf("登录成功，欢迎您，%s（%s）\n",
					g_current_user.username,
					g_current_user.is_admin ? "管理员" : "普通用户");
				log_action(g_current_user.username, "login", "success");
				return 1;
			}
		}
		
		printf("账号或密码错误，请重试。\n");
		try_times++;
	}
	
	log_action(u, "login", "failed");
	printf("错误次数过多，程序结束。\n");
	return 0;
}

/* ----------------------------- 数据文件 ----------------------------- */

static int get_next_id(void)
{
	FILE *fp;
	PrivacyRecord rec;
	int max_id;
	
	max_id = 0;
	fp = fopen(DATA_FILE, "rb");
	if (fp == NULL) {
		return 1;
	}
	while (fread(&rec, sizeof(PrivacyRecord), 1, fp) == 1) {
		if (rec.id > max_id) max_id = rec.id;
	}
	fclose(fp);
	return max_id + 1;
}

static int find_record_by_id(int target_id, PrivacyRecord *out_rec, long *out_pos)
{
	FILE *fp;
	PrivacyRecord rec;
	long pos;
	
	fp = fopen(DATA_FILE, "rb");
	if (fp == NULL) return 0;
	
	while (1) {
		pos = ftell(fp);
		if (fread(&rec, sizeof(PrivacyRecord), 1, fp) != 1) break;
		if (rec.id == target_id) {
			if (out_rec != NULL) *out_rec = rec;
			if (out_pos != NULL) *out_pos = pos;
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

static int update_record_at(long pos, const PrivacyRecord *rec)
{
	FILE *fp;
	if (rec == NULL) return 0;
	fp = fopen(DATA_FILE, "rb+");
	if (fp == NULL) return 0;
	if (fseek(fp, pos, SEEK_SET) != 0) {
		fclose(fp);
		return 0;
	}
	if (fwrite(rec, sizeof(PrivacyRecord), 1, fp) != 1) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	return 1;
}

/* ----------------------------- 功能实现 ----------------------------- */

static void add_record(void)
{
	PrivacyRecord rec;
	FILE *fp;
	char detail[128];
	
	printf("\n--- 新增隐私信息记录 ---\n");
	rec.id = get_next_id();
	printf("记录编号（自动生成）：%d\n", rec.id);
	
	printf("姓名: ");
	read_line(rec.owner_name, sizeof(rec.owner_name));
	
	printf("证件号/账号（如身份证/学号/用户名等）: ");
	read_line(rec.id_number, sizeof(rec.id_number));
	
	printf("联系电话: ");
	read_line(rec.phone, sizeof(rec.phone));
	
	printf("信息类型（如 账号、个人信息、文档 等）: ");
	read_line(rec.info_type, sizeof(rec.info_type));
	
	printf("备注（可空）: ");
	read_line(rec.remark, sizeof(rec.remark));
	rec.is_deleted = 0;
	
	fp = fopen(DATA_FILE, "ab");
	if (fp == NULL) {
		printf("写入数据文件失败！\n");
		return;
	}
	
	if (fwrite(&rec, sizeof(PrivacyRecord), 1, fp) != 1) {
		printf("写入数据失败！\n");
		fclose(fp);
		return;
	}
	fclose(fp);
	
	printf("保存成功。\n");
	sprintf(detail, "add id=%d owner=%s type=%s", rec.id, rec.owner_name, rec.info_type);
	log_action(g_current_user.username, "add_record", detail);
}

static void list_records_masked(void)
{
	FILE *fp;
	PrivacyRecord rec;
	char id_mask[MAX_ID_LEN];
	char phone_mask[MAX_PHONE_LEN];
	int count;
	
	fp = fopen(DATA_FILE, "rb");
	if (fp == NULL) {
		printf("当前没有任何记录。\n");
		return;
	}
	
	printf("\n--- 隐私信息列表（脱敏） ---\n");
	printf("%-4s %-10s %-20s %-15s %-10s\n",
		"ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("--------------------------------------------------------------\n");
	
	count = 0;
	while (fread(&rec, sizeof(PrivacyRecord), 1, fp) == 1) {
		if (rec.is_deleted) continue;
		mask_string(rec.id_number, id_mask, sizeof(id_mask), 3, 2);
		mask_string(rec.phone, phone_mask, sizeof(phone_mask), 3, 2);
		printf("%-4d %-10s %-20s %-15s %-10s\n",
			rec.id, rec.owner_name, id_mask, phone_mask, rec.info_type);
		count++;
	}
	fclose(fp);
	
	printf("共 %d 条记录。\n", count);
	log_action(g_current_user.username, "list_records", "masked");
}

static void view_record_detail(void)
{
	int target_id;
	FILE *fp;
	PrivacyRecord rec;
	int found;
	char detail[64];
	
	printf("\n--- 查看记录明细（查看明文） ---\n");
	printf("请输入记录编号 ID: ");
	if (scanf("%d", &target_id) != 1) {
		printf("输入错误。\n");
		clear_stdin_line();
		return;
	}
	clear_stdin_line();
	
	fp = fopen(DATA_FILE, "rb");
	if (fp == NULL) {
		printf("数据文件不存在。\n");
		return;
	}
	
	found = 0;
	while (fread(&rec, sizeof(PrivacyRecord), 1, fp) == 1) {
		if (rec.id == target_id) {
			if (rec.is_deleted) {
				printf("该记录已被软删除，请先恢复后查看。\n");
				found = 1;
				break;
			}
			printf("\n编号: %d\n", rec.id);
			printf("姓名: %s\n", rec.owner_name);
			printf("证件号/账号: %s\n", rec.id_number);
			printf("电话: %s\n", rec.phone);
			printf("类型: %s\n", rec.info_type);
			printf("备注: %s\n", rec.remark);
			found = 1;
			break;
		}
	}
	fclose(fp);
	
	if (!found) {
		printf("未找到编号为 %d 的记录。\n", target_id);
	}
	
	sprintf(detail, "view_detail id=%d %s", target_id, found ? "found" : "not_found");
	log_action(g_current_user.username, "view_detail", detail);
}

static void search_by_name(void)
{
	char key[MAX_NAME_LEN];
	FILE *fp;
	PrivacyRecord rec;
	int count;
	char detail[80];
	char id_mask[MAX_ID_LEN];
	char phone_mask[MAX_PHONE_LEN];
	
	printf("\n--- 按姓名搜索记录 ---\n");
	printf("请输入姓名关键字: ");
	read_line(key, sizeof(key));
	
	fp = fopen(DATA_FILE, "rb");
	if (fp == NULL) {
		printf("数据文件不存在。\n");
		return;
	}
	
	printf("%-4s %-10s %-20s %-15s %-10s\n",
		"ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("--------------------------------------------------------------\n");
	
	count = 0;
	while (fread(&rec, sizeof(PrivacyRecord), 1, fp) == 1) {
		if (rec.is_deleted) continue;
		if (strstr(rec.owner_name, key) != NULL) {
			mask_string(rec.id_number, id_mask, sizeof(id_mask), 3, 2);
			mask_string(rec.phone, phone_mask, sizeof(phone_mask), 3, 2);
			printf("%-4d %-10s %-20s %-15s %-10s\n",
				rec.id, rec.owner_name, id_mask, phone_mask, rec.info_type);
			count++;
		}
	}
	fclose(fp);
	
	if (count == 0) {
		printf("未找到相关记录。\n");
	} else {
		printf("共找到 %d 条记录。\n", count);
	}
	
	sprintf(detail, "search key=%s count=%d", key, count);
	log_action(g_current_user.username, "search_name", detail);
}

static void update_record(void)
{
	int target_id;
	int choice;
	long pos;
	PrivacyRecord rec;
	char detail[80];
	char input[64];
	
	printf("\n--- 修改记录 ---\n");
	printf("请输入要修改的记录编号 ID: ");
	if (scanf("%d", &target_id) != 1) {
		printf("输入错误。\n");
		clear_stdin_line();
		return;
	}
	clear_stdin_line();
	
	if (!find_record_by_id(target_id, &rec, &pos)) {
		printf("未找到记录。\n");
		return;
	}
	if (rec.is_deleted) {
		printf("该记录已软删除，不能修改，请先恢复。\n");
		return;
	}
	
	printf("当前姓名：%s\n", rec.owner_name);
	printf("输入新姓名（直接回车表示不改）: ");
	read_line(input, sizeof(input));
	if (strlen(input) > 0) strncpy(rec.owner_name, input, sizeof(rec.owner_name)-1);
	rec.owner_name[sizeof(rec.owner_name)-1] = '\0';
	
	printf("当前证件号/账号：%s\n", rec.id_number);
	printf("输入新证件号/账号（直接回车表示不改）: ");
	read_line(input, sizeof(input));
	if (strlen(input) > 0) strncpy(rec.id_number, input, sizeof(rec.id_number)-1);
	rec.id_number[sizeof(rec.id_number)-1] = '\0';
	
	printf("当前电话：%s\n", rec.phone);
	printf("输入新电话（直接回车表示不改）: ");
	read_line(input, sizeof(input));
	if (strlen(input) > 0) strncpy(rec.phone, input, sizeof(rec.phone)-1);
	rec.phone[sizeof(rec.phone)-1] = '\0';
	
	printf("当前类型：%s\n", rec.info_type);
	printf("输入新类型（直接回车表示不改）: ");
	read_line(input, sizeof(input));
	if (strlen(input) > 0) strncpy(rec.info_type, input, sizeof(rec.info_type)-1);
	rec.info_type[sizeof(rec.info_type)-1] = '\0';
	
	printf("当前备注：%s\n", rec.remark);
	printf("输入新备注（直接回车表示不改）: ");
	read_line(input, sizeof(input));
	if (strlen(input) > 0) strncpy(rec.remark, input, sizeof(rec.remark)-1);
	rec.remark[sizeof(rec.remark)-1] = '\0';
	
	printf("确认保存修改？(1=是, 0=否): ");
	if (scanf("%d", &choice) != 1) {
		printf("输入错误，取消修改。\n");
		clear_stdin_line();
		return;
	}
	clear_stdin_line();
	if (choice != 1) {
		printf("已取消。\n");
		return;
	}
	
	if (!update_record_at(pos, &rec)) {
		printf("保存失败。\n");
		return;
	}
	printf("修改成功。\n");
	sprintf(detail, "update id=%d", target_id);
	log_action(g_current_user.username, "update_record", detail);
}

static void soft_delete_record(void)
{
	int target_id;
	long pos;
	PrivacyRecord rec;
	char detail[80];
	
	if (!g_current_user.is_admin) {
		printf("只有管理员可以删除记录。\n");
		log_action(g_current_user.username, "delete_denied", "not_admin");
		return;
	}
	
	printf("\n--- 删除记录（管理员） ---\n");
	printf("请输入要删除的记录编号 ID: ");
	if (scanf("%d", &target_id) != 1) {
		printf("输入错误。\n");
		clear_stdin_line();
		return;
	}
	clear_stdin_line();
	
	if (!find_record_by_id(target_id, &rec, &pos)) {
		printf("未找到记录。\n");
		return;
	}
	if (rec.is_deleted) {
		printf("该记录已是删除状态。\n");
		return;
	}
	rec.is_deleted = 1;
	if (!update_record_at(pos, &rec)) {
		printf("删除失败。\n");
		return;
	}
	printf("记录 %d 已软删除。\n", target_id);
	sprintf(detail, "soft_delete id=%d", target_id);
	log_action(g_current_user.username, "soft_delete_record", detail);
}

static void restore_record(void)
{
	int target_id;
	long pos;
	PrivacyRecord rec;
	char detail[80];
	
	if (!g_current_user.is_admin) {
		printf("只有管理员可以恢复记录。\n");
		log_action(g_current_user.username, "restore_denied", "not_admin");
		return;
	}
	
	printf("\n--- 恢复记录（管理员） ---\n");
	printf("请输入要恢复的记录编号 ID: ");
	if (scanf("%d", &target_id) != 1) {
		printf("输入错误。\n");
		clear_stdin_line();
		return;
	}
	clear_stdin_line();
	
	if (!find_record_by_id(target_id, &rec, &pos)) {
		printf("未找到记录。\n");
		return;
	}
	if (!rec.is_deleted) {
		printf("该记录未被删除，无需恢复。\n");
		return;
	}
	
	rec.is_deleted = 0;
	if (!update_record_at(pos, &rec)) {
		printf("恢复失败。\n");
		return;
	}
	printf("记录 %d 已恢复。\n", target_id);
	sprintf(detail, "restore id=%d", target_id);
	log_action(g_current_user.username, "restore_record", detail);
}

static void list_deleted_records(void)
{
	FILE *fp;
	PrivacyRecord rec;
	int count;
	char id_mask[MAX_ID_LEN];
	char phone_mask[MAX_PHONE_LEN];
	
	if (!g_current_user.is_admin) {
		printf("只有管理员可以查看已删除记录。\n");
		return;
	}
	
	fp = fopen(DATA_FILE, "rb");
	if (fp == NULL) {
		printf("当前没有记录。\n");
		return;
	}
	printf("\n--- 已软删除记录 ---\n");
	printf("%-4s %-10s %-20s %-15s %-10s\n",
		"ID", "姓名", "证件号(脱敏)", "电话(脱敏)", "类型");
	printf("--------------------------------------------------------------\n");
	count = 0;
	while (fread(&rec, sizeof(PrivacyRecord), 1, fp) == 1) {
		if (!rec.is_deleted) continue;
		mask_string(rec.id_number, id_mask, sizeof(id_mask), 3, 2);
		mask_string(rec.phone, phone_mask, sizeof(phone_mask), 3, 2);
		printf("%-4d %-10s %-20s %-15s %-10s\n",
			rec.id, rec.owner_name, id_mask, phone_mask, rec.info_type);
		count++;
	}
	fclose(fp);
	printf("共 %d 条已删除记录。\n", count);
}

static void query_audit_log(void)
{
	FILE *fp;
	char line[256];
	char user_key[32];
	char action_key[32];
	int count;
	int user_ok, action_ok;
	
	printf("\n--- 审计日志查询 ---\n");
	printf("按用户筛选（留空=全部）: ");
	read_line(user_key, sizeof(user_key));
	printf("按动作筛选（留空=全部，例如 add_record）: ");
	read_line(action_key, sizeof(action_key));
	
	fp = fopen(AUDIT_FILE, "r");
	if (fp == NULL) {
		printf("暂无审计日志。\n");
		return;
	}
	
	count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		user_ok = 1;
		action_ok = 1;
		if (strlen(user_key) > 0) {
			char token_u[64];
			sprintf(token_u, "user=%s", user_key);
			if (strstr(line, token_u) == NULL) user_ok = 0;
		}
		if (strlen(action_key) > 0) {
			char token_a[64];
			sprintf(token_a, "action=%s", action_key);
			if (strstr(line, token_a) == NULL) action_ok = 0;
		}
		if (user_ok && action_ok) {
			printf("%s", line);
			count++;
		}
	}
	fclose(fp);
	printf("共匹配 %d 条日志。\n", count);
}

static void export_to_text(void)
{
	FILE *fp_in;
	FILE *fp_out;
	PrivacyRecord rec;
	char filename[64];
	int count;
	char detail[80];
	
	printf("\n--- 导出为文本 ---\n");
	printf("请输入导出文件名（例如 export.txt）: ");
	read_line(filename, sizeof(filename));
	if (filename[0] == '\0') {
		printf("文件名不能为空。\n");
		return;
	}
	
	fp_in = fopen(DATA_FILE, "rb");
	if (fp_in == NULL) {
		printf("数据文件不存在。\n");
		return;
	}
	fp_out = fopen(filename, "w");
	if (fp_out == NULL) {
		fclose(fp_in);
		printf("创建导出文件失败。\n");
		return;
	}
	
	fprintf(fp_out, "隐私信息导出（仅用于学习演示，严禁泄露）\n");
	fprintf(fp_out, "ID\t姓名\t证件号\t电话\t类型\t备注\n");
	
	count = 0;
	while (fread(&rec, sizeof(PrivacyRecord), 1, fp_in) == 1) {
		fprintf(fp_out, "%d\t%s\t%s\t%s\t%s\t%s\n",
			rec.id, rec.owner_name, rec.id_number,
			rec.phone, rec.info_type, rec.remark);
		count++;
	}
	
	fclose(fp_in);
	fclose(fp_out);
	
	printf("已导出 %d 条记录到文件：%s\n", count, filename);
	sprintf(detail, "export file=%s count=%d", filename, count);
	log_action(g_current_user.username, "export_text", detail);
}

static void print_menu(void)
{
	printf("\n============================================\n");
	printf("    隐私信息管理系统（C 单文件期末项目）\n");
	printf("    当前用户：%s（%s）\n",
		g_current_user.username,
		g_current_user.is_admin ? "管理员" : "普通用户");
	printf("============================================\n");
	printf("  1. 新增隐私信息记录\n");
	printf("  2. 列出所有记录（脱敏）\n");
	printf("  3. 查看记录明细（明文）\n");
	printf("  4. 按姓名搜索记录\n");
	printf("  5. 修改记录\n");
	printf("  6. 软删除记录（仅管理员）\n");
	printf("  7. 查看已删除记录（仅管理员）\n");
	printf("  8. 恢复记录（仅管理员）\n");
	printf("  9. 导出为文本文件\n");
	printf(" 10. 审计日志查询\n");
	printf("  0. 退出系统\n");
	printf("============================================\n");
	printf("请输入选项: ");
}

int main(void)
{
	int choice;
	int running;
	
	running = 1;
	if (!login()) {
		return 0;
	}
	
	while (running) {
		print_menu();
		if (scanf("%d", &choice) != 1) {
			printf("输入无效，请输入数字。\n");
			clear_stdin_line();
			continue;
		}
		clear_stdin_line();
		
		switch (choice) {
		case 1:
			add_record();
			break;
		case 2:
			list_records_masked();
			break;
		case 3:
			view_record_detail();
			break;
		case 4:
			search_by_name();
			break;
		case 5:
			update_record();
			break;
		case 6:
			soft_delete_record();
			break;
		case 7:
			list_deleted_records();
			break;
		case 8:
			restore_record();
			break;
		case 9:
			export_to_text();
			break;
		case 10:
			query_audit_log();
			break;
		case 0:
			printf("系统退出，再见。\n");
			log_action(g_current_user.username, "logout", "normal");
			running = 0;
			break;
		default:
			printf("无效选项，请重新输入。\n");
			break;
		}
	}
	
	return 0;
}

