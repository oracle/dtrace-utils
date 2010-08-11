#ifndef _DTRACE_H_
#define _DTRACE_H_

#include <linux/cred.h>
#include <linux/types.h>
#include <linux/stringify.h>

#define DTRACE_PROVNAMELEN	64
#define DTRACE_MODNAMELEN	64
#define DTRACE_FUNCNAMELEN	128
#define DTRACE_NAMELEN		64

#define DTRACE_ARGTYPELEN	128

#define DTRACE_PRIV_NONE	0x0000
#define DTRACE_PRIV_KERNEL	0x0001
#define DTRACE_PRIV_USER	0x0002
#define DTRACE_PRIV_PROC	0x0004
#define DTRACE_PRIV_OWNER	0x0008

typedef uint8_t		dtrace_stability_t;
typedef uint8_t		dtrace_class_t;

typedef struct dtrace_attribute {
	dtrace_stability_t dtat_name;
	dtrace_stability_t dtat_data;
	dtrace_class_t dtat_class;
} dtrace_attribute_t;

typedef struct dtrace_pattr {
	dtrace_attribute_t dtpa_provider;
	dtrace_attribute_t dtpa_mod;
	dtrace_attribute_t dtpa_func;
	dtrace_attribute_t dtpa_name;
	dtrace_attribute_t dtpa_args;
} dtrace_pattr_t;

typedef uint32_t dtrace_id_t;

typedef struct dtrace_probedesc {
	dtrace_id_t dtpd_id;
	char dtpd_provider[DTRACE_PROVNAMELEN];
	char dtpd_mod[DTRACE_MODNAMELEN];
	char dtpd_func[DTRACE_FUNCNAMELEN];
	char dtpd_name[DTRACE_NAMELEN];
} dtrace_probedesc_t;

typedef struct dtrace_argdesc {
	dtrace_id_t dtargd_id;
	int dtargd_ndx;
	int dtargd_mapping;
	char dtargd_native[DTRACE_ARGTYPELEN];
	char dtargd_xlate[DTRACE_ARGTYPELEN];
} dtrace_argdesc_t;

typedef struct dtrace_pops {
	void (*dtps_provide)(void *, const dtrace_probedesc_t *);
	void (*dtps_provide_module)(void *, void *);
	int (*dtps_enable)(void *, dtrace_id_t, void *);
	void (*dtps_disable)(void *, dtrace_id_t, void *);
	void (*dtps_suspend)(void *, dtrace_id_t, void *);
	void (*dtps_resume)(void *, dtrace_id_t, void *);
	void (*dtps_getargdesc)(void *, dtrace_id_t, void *,
				dtrace_argdesc_t *);
	uint64_t (*dtps_getargval)(void *, dtrace_id_t, void *, int, int);
	int (*dtps_usermode)(void *, dtrace_id_t, void *);
	void (*dtps_destroy)(void *, dtrace_id_t, void *);
} dtrace_pops_t;

typedef struct dtrace_helper_probedesc {
	char *dthpb_mod;
	char *dthpb_func;
	char *dthpb_name;
	uint64_t dthpb_base;
	uint32_t *dthpb_offs;
	uint32_t *dthpb_enoffs;
	uint32_t dthpb_noffs;
	uint32_t dthpb_nenoffs;
	uint8_t *dthpb_args;
	uint8_t dthpb_xargc;
	uint8_t dthpb_nargc;
	char *dthpb_xtypes;
	char *dthpb_ntypes;
} dtrace_helper_probedesc_t;

typedef struct dtrace_helper_provdesc {
	char *dthpv_provname;
	dtrace_pattr_t dthpv_pattr;
} dtrace_helper_provdesc_t;

typedef struct dtrace_mops {
	void (*dtms_create_probe)(void *, void *, dtrace_helper_probedesc_t *);
	void (*dtms_provide_pid)(void *, dtrace_helper_provdesc_t *, pid_t);
	void (*dtms_remove_pid)(void *, dtrace_helper_provdesc_t *, pid_t);
} dtrace_mops_t;

/*
 * DTrace Provider-to-Framework API Functions
 */
typedef uintptr_t	dtrace_provider_id_t;
typedef struct cred	cred_t;

extern int dtrace_register(const char *, const dtrace_pattr_t *, uint32_t,
			   cred_t *, const dtrace_pops_t *, void *,
			   dtrace_provider_id_t *);
extern int dtrace_unregister(dtrace_provider_id_t);

/*
 * DTrace Meta Provider-to-Framework API Functions
 */
typedef uintptr_t	dtrace_meta_provider_id_t;

extern int dtrace_meta_register(const char *, const dtrace_mops_t *, void *,
				dtrace_meta_provider_id_t *);
extern int dtrace_meta_unregister(dtrace_meta_provider_id_t);

#define DT_PROVIDER_MODULE(name)					\
  static dtrace_provider_id_t name##_id;				\
									\
  static int __init name##_init(void)					\
  {									\
	int ret = 0;							\
									\
	ret = name##_dev_init();					\
	if (ret)							\
		goto failed;						\
									\
	ret = dtrace_register(__stringify(name), &name##_attr,		\
			      DTRACE_PRIV_USER, NULL, &name##_pops,	\
			      NULL, &name##_id);			\
	if (ret)							\
		goto failed;						\
									\
	return 0;							\
									\
  failed:								\
	return ret;							\
  }									\
									\
  static void __exit name##_exit(void)					\
  {									\
	dtrace_unregister(name##_id);					\
	name##_dev_exit();						\
  }									\
									\
  module_init(name##_init);						\
  module_exit(name##_exit);

#endif /* _DTRACE_H_ */
