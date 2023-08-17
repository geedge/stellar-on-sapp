#include "stellar_internal.h"
#include "session_manager.h"

#include "stellar/utils.h"
#include "stellar/session_exdata.h"


struct plugin_specific_inernal
{
	struct plugin_specific specs;
	void *plugin_ctx;
};

UT_icd plugin_specs_icd = {sizeof(struct plugin_specific_inernal), NULL, NULL, NULL};

static void exdata_plugin_rt_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
	if(ex_ptr)
	{
		FREE(ex_ptr);
	}
	return;
}


struct stellar *stellar_init(struct plugin_specific specs[] , int spec_num)
{
	struct stellar *st = CALLOC(struct stellar, 1);
	struct plugin_specific_inernal spec_internal={};
	utarray_new(st->plugin_specs_array,&plugin_specs_icd);
	utarray_reserve(st->plugin_specs_array, spec_num);
	st->intrinsic_session_event_exdata_idx=stellar_session_get_ex_new_index(st, "__STELLAR__", exdata_plugin_rt_free, NULL);
	for(int i = 0; i < spec_num; i++)
	{
		if (specs[i].init_cb != NULL)
		{
			spec_internal.plugin_ctx=specs[i].init_cb(st);
			if(spec_internal.plugin_ctx == NULL)
			{
				goto ADAPTER_INIT_ERROR;
			}
			spec_internal.specs=specs[i];
			utarray_push_back(st->plugin_specs_array, &spec_internal);
		}
	}
	return st;
ADAPTER_INIT_ERROR:
	stellar_exit(st);
	return NULL;
}

void stellar_exit(struct stellar *st)
{
	struct plugin_specific_inernal *p=NULL;
	while ((p = (struct plugin_specific_inernal *)utarray_next(st->plugin_specs_array, p)))
	{
		if(p->specs.exit_cb != NULL)
		{
			p->specs.exit_cb(p->plugin_ctx);
		}
	}
	utarray_free(st->session_mq_schema_array);
	utarray_free(st->session_exdata_schema_array);
	utarray_free(st->intrinsic_session_event_schema_array);
	utarray_free(st->plugin_specs_array);
	FREE(st);
	return;	
};