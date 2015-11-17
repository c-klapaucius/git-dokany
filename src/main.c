#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <crtdbg.h>

#include "win/win.h"

#include "fusedgit.h"

#include "config.h"

static int mounted;
static fusedgit_t fusedgit;

static void umount(void);
static void mount(void)
{
	winCfgLock();
	umount();
	cfgSane();
	printf("Mounting ...\n");
	if (!fusedgit) {
		unsigned long long cacheSize=cfg.cacheSizeMB;
		cacheSize*=1000000;
		int threads=4;
		printf("Intitializing: threads=%d, cache size = %llu\n", threads, cacheSize);
		fusedgit=fusedgit_create(threads, cacheSize);
	}
	cfgEnumReset();
	const char *repo, *prevRepo=NULL;
	const char *treeish;
	const char *mapto;
	fusedgit_repo_t fgitRepo=NULL;
	int haveTree=0;
	while (1) {
		cfgEnumNextMount(&repo, &treeish, &mapto);
		if (!repo)
			break;
		if (!prevRepo || strcmp(prevRepo, repo)) {
			prevRepo=repo;
			printf("Adding repo: %s\n", repo);
			fusedgit_releaserepo(fgitRepo);
			fgitRepo=fusedgit_addrepo(fusedgit, repo);
			if (!fgitRepo)
				printf("Faled adding repo: %s\n", repo);
		}
		if (treeish && mapto) {
			if (!fgitRepo) {
				printf("No repo: not mapping '%s' to '%s'\n", treeish, mapto);
			} else {
				printf("Mapping '%s' to '%s'\n", treeish, mapto);
				int res=fusedgit_addtree(fgitRepo, treeish, mapto);
				if (res)
					printf("Failed to map '%s' to '%s'\n", treeish, mapto);
				else
					haveTree=1;
			}
		}
	}
	fusedgit_releaserepo(fgitRepo);
	if (!haveTree) {
		printf("Nothing mapped, not mounting drive\n");
	} else {
		printf("Mounting drive %s\n", cfg.drive);
		int res=fusedgit_mount(fusedgit, cfg.drive);
		if (res) {
			printf("Failed to mount (%d), freeing data\n", res);
			fusedgit_destroy(fusedgit);
			fusedgit=NULL;
		} else {
			printf("Drive mounted\n");
			mounted=1;
		}
	}
	winCfgUnlock();
}

static void umount(void)
{
	if (mounted) {
		mounted=0;
		printf("Unmounting drive\n");
		int res=fusedgit_umount(fusedgit);
		if (res)
			printf("Failed to unmount\n");
	}
	printf("Freeing data\n");
	fusedgit_destroy(fusedgit);
	printf("data freed\n");
	fusedgit=NULL;
}

static void usage(void)
{
	printf(
		"Usage:\n"
		"  gitdokany <--help|-h>            display this help and exit\n"
		"  gitdokany <config_file>          use specified config file\n"
		"  gitdokany <option> <opton> ...   specify command line options\n"
		"    drive=L:                       drive to mount to (drive=X: drive=Y)\n"
		"    repo=C:\\path\\to\\repo           repository location (repo=C:\\torvalds\\linux.git)\n"
		"    treeish=TREEISH                git tree-ish to map (treeish=master treeish=HEAD:samples/fuse_mirror\n"
		"    path=<map_path>                path to map tree-ish to, relative to mount drive (path=debug\\master)\n"
		"    gui=0/1/2                      0 - no gui, 1 - gui (hide console), 2 - gui (show console)\n"
		"    cache=100                      cache size (MB)\n"
		"    cfg=config_file                parse additional options from specified file, one option per line\n"
		"  The 'path=' option supports <hash> and <treeish> tags:\n"
		"    path=debug\\<hash>\\<treeish>\n"
		"\n"
		"\n"
	);
}

int main(int argc, char *argv[])
{
	int hasDokan=dokanDllPresent();
	cfgReset();
	if (argc==2) {
		if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
			usage();
			return 0;
		}
		cfgLoad(argv[1], 0);
	} else {
		if (argc>2) {
			cfgAddRepo("."); // current directory by default
			cfg.gui=0;
		}
		for (int i=1; i<argc; i++) {
			if (cfgParseOpt(argv[i])) {
				usage();
				return 1;
			}
		}
	}
	cfgSane();
	if (cfg.gui) {
		if (winCreate()) {
			printf("Failed to create GUI\n");
			return 1;
		}
	}
	if (!hasDokan) {
		printf(
			"It seems that the DOKANY system driver is not present.\n"
			"This program requires the DOKANY driver to operate.\n"
		);
	}
	if (cfg.gui) {
		if (argc==2)
			mount();
		while (1) {
			int cmd=winCmdGet(mounted);
			if (cmd==winCmd_exit)
				break;
			if (cmd==winCmd_mount)
				mount();
			if (cmd==winCmd_umount)
				umount();
		}
	} else {
		mount();
		printf("Press ENTER to unmount and exit...");
		getchar();
	}
	printf("Exiting...\n");
	umount();
	cfgReset();
	_CrtDumpMemoryLeaks();
	return 0;
}