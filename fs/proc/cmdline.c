// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_SEC_DETECT_CMDLINE_PATCH
#include <linux/string.h> // Include for string manipulation functions
#include <linux/sec_detect.h>

#define BUF_SIZE 4096   // Define a buffer size

static void cmdline_panel_patch(char *command_line)
{
	char buf[BUF_SIZE];
	char *pos = buf;
	char *token;
	const char *delim = " ";
	int offset = 0;

	// Initialize an empty buffer
	memset(buf, 0, BUF_SIZE);

	// Copy the command_line to the buffer
	strncpy(buf, command_line, BUF_SIZE - 1);

	// Tokenize the buffer to process each entry
	while ((token = strsep(&pos, delim)) != NULL) {
		if (sec_needs_decon) {
			if (strstr(token, "mcd-panel.") == token) {
				offset += snprintf(command_line + offset, BUF_SIZE - offset, "mcd-panel-decon%s ", token + strlen("mcd-panel"));
			} else if (strstr(token, "exynos-drm.") == token) {
				offset += snprintf(command_line + offset, BUF_SIZE - offset, "exynos-drm-decon%s ", token + strlen("exynos-drm"));
			} else if (strstr(token, "mcd-panel-samsung-drv.") == token) {
				offset += snprintf(command_line + offset, BUF_SIZE - offset, "mcd-panel-samsung-drv-decon%s ", token + strlen("mcd-panel-samsung-drv"));
			} else {
				offset += snprintf(command_line + offset, BUF_SIZE - offset, "%s ", token);
			}
		} else {
			if (strstr(token, "mcd-panel-samsung-drv.") == token) {
				offset += snprintf(command_line + offset, BUF_SIZE - offset, "mcd-panel-samsung-drv-usdm%s ", token + strlen("mcd-panel-samsung-drv"));
			} else {
				offset += snprintf(command_line + offset, BUF_SIZE - offset, "%s ", token);
			}
		}
	}

	// Remove the trailing space
	if (offset > 0 && command_line[offset - 1] == ' ')
		command_line[offset - 1] = '\0';
}
#endif

static int cmdline_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_SEC_DETECT_CMDLINE_PATCH
	// Modify the saved_command_line before showing it
	cmdline_panel_patch(saved_command_line);
#endif

	seq_puts(m, saved_command_line);
	seq_putc(m, '\n');
	return 0;
}

static int __init proc_cmdline_init(void)
{
	proc_create_single("cmdline", 0, NULL, cmdline_proc_show);
	return 0;
}
fs_initcall(proc_cmdline_init);
