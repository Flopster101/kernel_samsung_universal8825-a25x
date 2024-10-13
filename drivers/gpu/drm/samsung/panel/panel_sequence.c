#include <linux/kernel.h>
#include <linux/types.h>
#include "panel.h"
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"
#include "panel_sequence.h"

bool is_valid_sequence(struct seqinfo *seq)
{
	if (!seq)
		return false;

	if (!IS_CMD_TYPE_SEQ(get_pnobj_cmd_type(&seq->base)))
		return false;

	if (seq->cmdtbl == NULL)
		return false;

	return true;
}

char *get_sequence_name(struct seqinfo *seq)
{
	return get_pnobj_name(&seq->base);
}

int snprintf_sequence(char *buf, size_t size, struct seqinfo *seq)
{
	int i, len;

	if (!seq)
		return 0;

	len = snprintf(buf, size, "%s\n", get_sequence_name(seq));
	len += snprintf(buf + len, size - len, "commands: %d\n", seq->size);
	for (i = 0; i < seq->size; i++) {
		if (!seq->cmdtbl[i])
			break;
		len += snprintf(buf + len, size - len, "[%d]: type: %8s, name: %s",
				i, cmd_type_to_string(get_pnobj_cmd_type(seq->cmdtbl[i])),
				get_pnobj_name(seq->cmdtbl[i]));
		if (i + 1 != seq->size)
			len += snprintf(buf + len, size - len, "\n");
	}
	return len;
}

/**
 * create_sequence - create a struct seqinfo structure
 * @name: pointer to a string for the name of this sequence.
 * @size : number of commands.
 *
 * This is used to create a struct seqinfo pointer.
 *
 * Returns &struct seqinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_sequence().
 */
struct seqinfo *create_sequence(char *name, size_t size)
{
	struct seqinfo *seq;

	if (!name)
		return NULL;

	if (!size)
		return NULL;

	seq = kzalloc(sizeof(*seq), GFP_KERNEL);
	if (!seq)
		return NULL;

	seq->cmdtbl = kzalloc(size * sizeof(void *), GFP_KERNEL);
	if (!seq->cmdtbl)
		goto err;

	pnobj_init(&seq->base, CMD_TYPE_SEQ, name);
	seq->size = size;

	return seq;

err:
	kfree(seq);
	return NULL;
}
EXPORT_SYMBOL(create_sequence);

/**
 * destroy_sequence - destroys a struct seqinfo structure
 * @seq: pointer to the struct seqinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_sequence().
 */
void destroy_sequence(struct seqinfo *seq)
{
	if (!seq)
		return;

	pnobj_deinit(&seq->base);
	kfree(seq->cmdtbl);
	kfree(seq);
}
EXPORT_SYMBOL(destroy_sequence);

__visible_for_testing int add_edge(struct list_head *head, int vertex)
{
	struct command_node *node;

	if (!head)
		return -EINVAL;

	if (vertex < 0)
		return -EINVAL;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->vertex = vertex;
	list_add_tail(&node->list, head);

	return 0;
}

/* TODO: need depth limit */
__visible_for_testing void topo_sort(struct list_head adj_list[],
		int vertex, bool visited[], int stack[], int *top)
{
	struct command_node *pos;

	visited[vertex] = true;
	list_for_each_entry(pos, &adj_list[vertex], list) {
		if (!visited[pos->vertex])
			topo_sort(adj_list, pos->vertex, visited, stack, top);
	}
	stack[(*top)++] = vertex;
}

__visible_for_testing bool check_cycle(struct list_head adj_list[],
		int n, int stack[], int *top)
{
	int i, *order;

	order = kmalloc_array(n, sizeof(*order), GFP_KERNEL);
	for (i = 0; i < n; i++)
		order[stack[*top - i - 1]] = i;

	for (i = 0; i < n; i++) {
		struct command_node *pos;

		list_for_each_entry(pos, &adj_list[i], list) {
			if (order[i] > order[pos->vertex]) {
				kfree(order);
				return true;
			}
		}
	}
	kfree(order);

	return false;
}

__visible_for_testing bool is_cyclic_util(struct list_head *adj_list,
		int v, bool visited[], bool *recur_stack)
{
	struct command_node *pos;

	if (!visited[v]) {
		visited[v] = true;
		recur_stack[v] = true;

		list_for_each_entry(pos, &adj_list[v], list) {
			if (!visited[pos->vertex] &&
					is_cyclic_util(adj_list, pos->vertex, visited, recur_stack))
				return true;
			else if (recur_stack[pos->vertex])
				return true;
		}
	}
	recur_stack[v] = false;

	return false;
}

__visible_for_testing bool is_cyclic(struct list_head *adj_list, int n)
{
	bool *visited, *recur_stack;
	bool cycle = false;
	int i;

	visited = kzalloc(sizeof(*visited) * n, GFP_KERNEL);
	recur_stack = kzalloc(sizeof(*recur_stack) * n, GFP_KERNEL);

	for (i = 0; i < n; i++) {
		if (!visited[i] &&
				is_cyclic_util(adj_list, i, visited, recur_stack)) {
			cycle = true;
			goto out;
		}
	}

out:
	kfree(visited);
	kfree(recur_stack);

	return cycle;
}

/* TODO: change to hash table search */
int get_index_of_sequence(struct list_head *seq_list,
		struct seqinfo *sequence)
{
	struct pnobj *pos;
	int index = 0;

	if (!sequence)
		return -1;

	list_for_each_entry(pos, seq_list, list) {
		if (pos == &sequence->base)
			return index;
		index++;
	}

	return -1;
}

int get_count_of_sequence(struct list_head *seq_list)
{
	int num_seq = 0;
	struct pnobj *pos;

	list_for_each_entry(pos, seq_list, list) {
		if (!is_valid_sequence(
					pnobj_container_of(pos, struct seqinfo)))
			continue;

		num_seq++;
	}

	return num_seq;
}

int sequence_sort(struct list_head *seq_list)
{
	struct list_head *adj_list;
	bool *visited;
	int *stack;
	int i, iseq = 0, index, top = 0, ret = 0;
	unsigned int num_seq;
	struct seqinfo *sequence;
	struct seqinfo **seq_array;
	struct pnobj *pos, *command;

	num_seq = get_count_of_sequence(seq_list);
	adj_list = kzalloc(sizeof(*adj_list) * num_seq, GFP_KERNEL);
	visited = kzalloc(sizeof(*visited) * num_seq, GFP_KERNEL);
	stack = kzalloc(sizeof(*stack) * num_seq, GFP_KERNEL);
	seq_array = kmalloc_array(num_seq, sizeof(struct seqinfo *), GFP_KERNEL);

	for (i = 0; i < num_seq; i++)
		INIT_LIST_HEAD(&adj_list[i]);

	/* add_edge */
	list_for_each_entry(pos, seq_list, list) {
		sequence = pnobj_container_of(pos, struct seqinfo);
		if (!is_valid_sequence(sequence)) {
			panel_err("invalid sequence(%s)\n", get_sequence_name(sequence));
			ret = -EINVAL;
			goto err;
		}

		seq_array[iseq] = sequence;
		for (i = 0; i < sequence->size && sequence->cmdtbl[i]; i++) {
			command = sequence->cmdtbl[i];
			if (!IS_CMD_TYPE_SEQ(get_pnobj_cmd_type(command)))
				continue;

			index = get_index_of_sequence(seq_list,
					pnobj_container_of(command, struct seqinfo));
			if (index < 0) {
				panel_err("failed to get index_of_sequnece(%s:%d:%s)\n",
						get_sequence_name(sequence), index, get_pnobj_name(command));
				ret = -EINVAL;
				goto err;
			}

			ret = add_edge(&adj_list[iseq], index);
			if (ret < 0) {
				panel_err("failed to add_edge(%s:%d:%s)\n",
						get_sequence_name(sequence), index, get_pnobj_name(command));
				goto err;
			}
		}
		iseq++;
	}

	if (is_cyclic(adj_list, num_seq)) {
		panel_err("cycle exists\n");
		ret = -EINVAL;
		goto err;
	}

	/* topological sorting */
	for (i = 0; i < num_seq; i++) {
		if (!visited[i])
			topo_sort(adj_list, i, visited, stack, &top);
	}

	if (check_cycle(adj_list, num_seq, stack, &top)) {
		panel_err("unsorted order detected\n");
		ret = -EINVAL;
		goto err;
	}

	/* make sorted sequence list */
	INIT_LIST_HEAD(seq_list);
	for (i = 0; i < num_seq; i++)
		list_add_tail(get_pnobj_list(&seq_array[stack[i]]->base),
				seq_list);

err:
	/* free command_node */
	for (i = 0; i < num_seq; i++) {
		struct command_node *pos, *next;

		list_for_each_entry_safe(pos, next, &adj_list[i], list)
			kfree(pos);
	}

	kfree(adj_list);
	kfree(visited);
	kfree(stack);
	kfree(seq_array);

	return ret;
}

struct pnobj_refs *sequence_to_pnobj_refs(struct seqinfo *seq)
{
	struct pnobj_refs *pnobj_refs;
	struct pnobj *pnobj;
	int i, ret;

	if (!seq)
		return NULL;

	pnobj_refs = create_pnobj_refs();
	if (!pnobj_refs)
		return NULL;

	for (i = 0; i < seq->size; i++) {
		pnobj = (struct pnobj *)seq->cmdtbl[i];
		if (!pnobj)
			break;

		if (!is_valid_panel_obj(pnobj)) {
			panel_warn("invalid pnobj(%s)\n",
					get_pnobj_name(pnobj));
			break;
		}

		ret = add_pnobj_ref(pnobj_refs, pnobj);
		if (ret < 0)
			goto err;
	}

	return pnobj_refs;

err:
	remove_pnobj_refs(pnobj_refs);
	return NULL;
}

struct pnobj_refs *sequence_filter(bool (*f)(struct pnobj *), struct seqinfo *seq)
{
	struct pnobj_refs *cmd_refs;
	struct pnobj_refs *filtered_refs;

	if (!f || !seq)
		return NULL;

	cmd_refs = sequence_to_pnobj_refs(seq);
	if (!cmd_refs)
		return NULL;

	filtered_refs = pnobj_refs_filter(f, cmd_refs);
	remove_pnobj_refs(cmd_refs);

	return filtered_refs;
}

static bool check_skip_condition(u32 type, int *cond_skip_count)
{
	bool skip = true;

	if (type == CMD_TYPE_COND_IF)
		(*cond_skip_count)++;
	else if (type == CMD_TYPE_COND_EL)
		skip = (*cond_skip_count > 0);
	else if (type == CMD_TYPE_COND_FI)
		(*cond_skip_count)--;

	return skip;
}

struct pnobj_refs *sequence_condition_filter(struct panel_device *panel, struct seqinfo *seq)
{
	struct pnobj_refs *orig_refs;
	struct pnobj_refs *filtered_refs;
	struct pnobj_ref *ref;
	struct pnobj *pnobj;
	bool condition = true;
	int ret, cond_skip_count = 0;
	u32 type;

	orig_refs = sequence_to_pnobj_refs(seq);
	if (!orig_refs)
		return NULL;

	filtered_refs = create_pnobj_refs();
	if (!filtered_refs)
		return NULL;

	list_for_each_entry(ref, &orig_refs->list, list) {
		pnobj = ref->pnobj;
		type = get_pnobj_cmd_type(pnobj);

		if (!condition && check_skip_condition(type, &cond_skip_count))
			continue;

		if (type == CMD_TYPE_COND_IF) {
			condition = panel_do_condition(panel,
					pnobj_container_of(pnobj, struct condinfo));
		} else if (type == CMD_TYPE_COND_EL) {
			condition = !condition;
		} else if (type == CMD_TYPE_COND_FI) {
			condition = true;
		} else {
			ret = add_pnobj_ref(filtered_refs, pnobj);
			if (ret < 0)
				goto err;
		}
	}

	remove_pnobj_refs(orig_refs);
	return filtered_refs;

err:
	remove_pnobj_refs(filtered_refs);
	remove_pnobj_refs(orig_refs);
	return NULL;
}

int get_packet_refs_from_sequence(struct panel_device *panel,
		char *seqname, struct pnobj_refs *pnobj_refs)
{
	struct pnobj_refs *cmd_refs;
	struct pnobj_refs *pkt_refs;
	struct seqinfo *seq;

	if (!panel || !seqname)
		return -EINVAL;

	if (!pnobj_refs) {
		panel_err("null pnobj_refs\n");
		return -EINVAL;
	}

	if (!check_seqtbl_exist(panel, seqname)) {
		panel_info("seq name: %s does not exists\n", seqname);
		return 0;
	}

	seq = find_panel_seq_by_name(panel, seqname);
	if (!seq) {
		panel_err("sequence(%s) not found\n", seqname);
		return -EINVAL;
	}

	cmd_refs = sequence_condition_filter(panel, seq);
	pkt_refs = pnobj_refs_filter(is_tx_packet, cmd_refs);

	list_replace_init(get_pnobj_refs_list(pkt_refs),
			get_pnobj_refs_list(pnobj_refs));

	remove_pnobj_refs(cmd_refs);
	remove_pnobj_refs(pkt_refs);

	return 0;
}
