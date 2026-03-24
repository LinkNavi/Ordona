#include "git.h"
#include "ordona.h"
#include "console.h"
std::string build_repo_url(std::string repo){
// note: takes for example LinkNavi/WindowsGnu

return "https://www.github.com/" + repo;
}

std::string repo_name(std::string repo){
    size_t pos = repo.find('/');
    return  pos != std::string::npos ? repo.substr(pos + 1) : repo;
}

void clone_repo(std::string repo){
    std::string dest = ordona_dir() + "plugins/src/" + repo_name(repo);
    repo = build_repo_url(repo);

    std::string cmd = "git clone " + repo + " " + dest;
    execute_cmd(cmd); 
}

