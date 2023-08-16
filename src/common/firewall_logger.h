#include <stddef.h>

struct firewall_logger;
void firewall_logger_send(struct firewall_logger *instance, int topic_id, const char *log, size_t log_len);
int firewall_logger_register_topic(struct firewall_logger *instance, const char *topic_name);
#define	LOG_TOPIC_MAX	128
struct firewall_logger_stat_info
{
	int topic_id[LOG_TOPIC_MAX];
	long long tx_logs[LOG_TOPIC_MAX];
	long long tx_errors[LOG_TOPIC_MAX];
	long long tx_dropped[LOG_TOPIC_MAX];
};
void firewall_logger_get_stat_info(struct firewall_logger *instance, struct firewall_logger_stat_info *stat_info);