#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int readBPFFile(const char *fn)
{
	int		fd;
	int		rc = -1;
	int		idx = 0;
	int		text_shidx = 0;
	size_t		text_size = 0, offset = 0;
	Elf		*elf = NULL;
	Elf_Data	*data;
	Elf_Scn		*scn;
	GElf_Ehdr	ehdr;
	GElf_Shdr	shdr;

	if ((fd = open64(fn, O_RDONLY)) == -1) {
		fprintf(stderr, "Failed to open %s: %s\n",
			fn, strerror(errno));
		return -1;
	}
	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "Failed to open ELF in %s\n", fn);
		goto err_elf;
	}
	if (elf_kind(elf) != ELF_K_ELF) {
		fprintf(stderr, "Unsupported ELF file type for %s: %d\n",
			fn, elf_kind(elf));
		goto out;
	}
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		fprintf(stderr, "Failed to read EHDR from %s", fn);
		goto err_elf;
	}
	if (ehdr.e_machine != EM_BPF) {
		fprintf(stderr, "Unsupported ELF machine type %d for %s\n",
			ehdr.e_machine, fn);
		goto out;
	}

	scn = NULL;
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		char *name;

		idx++;
		if (gelf_getshdr(scn, &shdr) == NULL) {
			fprintf(stderr, "Failed to get SHDR from %s", fn);
			goto err_elf;
		}

		name = elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name);
		if (name == NULL) {
			fprintf(stderr, "Failed to get section name from %s\n",
				fn);
			goto out;
		}
printf("Found section [%s], idx %d\n", name, idx);
		if (shdr.sh_type == SHT_SYMTAB) {
			size_t	i;

			if ((data = elf_getdata(scn, NULL)) == NULL) {
				fprintf(stderr, "Failed to get .symtab data "
						"from %s", fn);
				goto err_elf;
			}

			for (i = 0; i < data->d_size / sizeof(GElf_Sym); i++) {
				GElf_Sym	sym;
				char		*sname;

				if (!gelf_getsym(data, i, &sym))
					continue;
				if (GELF_ST_BIND(sym.st_info) != STB_GLOBAL)
					continue;
				if (sym.st_shndx != text_shidx)
					continue;
				sname = elf_strptr(elf, shdr.sh_link,
						   sym.st_name);
				if (sname == NULL) {
					fprintf(stderr, "Failed to get symbol "
							"name from %s\n", fn);
					goto out;
				}
if (sym.st_value > offset) printf("    Updated previous symbol size = %lu (%lu dw)\n", sym.st_value - offset, (sym.st_value - offset) / 8);
offset = sym.st_value;
printf("  Found symbol [%s], size = %lu, val = %lu, bind %d, type %d, section %d\n", sname, sym.st_size != 0 ? sym.st_size : text_size - sym.st_value, sym.st_value, GELF_ST_BIND(sym.st_info), GELF_ST_TYPE(sym.st_info), sym.st_shndx);
			}
                } else if (shdr.sh_type == SHT_PROGBITS) {
			if ((data = elf_getdata(scn, NULL)) == NULL) {
				fprintf(stderr, "Failed to get data from %s",
					fn);
				goto err_elf;
			}
			if (!(shdr.sh_flags & SHF_EXECINSTR))
				continue;

			if (strcmp(name, ".text") == 0) {
				text_shidx = idx;
				text_size = shdr.sh_size;
			}
		}
	}

	rc = 0;
	goto out;

err_elf:
	fprintf(stderr, ": %s\n", elf_errmsg(elf_errno()));

out:
	if (elf)
		elf_end(elf);

	close(fd);

	return rc;
}

int main(int argc, char *argv[])
{
	int	i;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "Invalid current ELF version.\n");
		exit(1);
	}
	if (argc < 2) {
		fprintf(stderr, "No object files specified.\n");
		exit(1);
	}

	for (i = 1; i < argc; i++)
		readBPFFile(argv[i]);
}
