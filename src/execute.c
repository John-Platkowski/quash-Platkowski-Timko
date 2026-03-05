/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */
#define _GNU_SOURCE
#include "execute.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "quash.h"
#include <assert.h>
#include "deque.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

// Remove this and all expansion calls to it
/**execvpPlease submit through your lab section's Canva
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
  fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)

#define FINISH_ME()                                                  \
  fprintf(stderr, "FINISH ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)



/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current workexecvping directory.
char* get_current_directory(bool* should_free) 
{ 
  *should_free = true;

  return get_current_dir_name();
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) 
{
  // Lookup environment variables. This is requiexecvpred for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly

  return getenv(env_var);
}



typedef struct job_s {
  int job_id;
  pid_t* pids;
  int num_pids;
  char* cmd_str;
  bool job_complete;
} Job;

IMPLEMENT_DEQUE_STRUCT(jobs_queue, Job);
IMPLEMENT_DEQUE(jobs_queue, Job);
size_t max_queue_length = 20;
jobs_queue jobs_queue_g;



void destroy_job(Job job)
{
  free(job.pids);
  free(job.cmd_str);
}

void init_jobs()
{
  jobs_queue_g = new_destructable_jobs_queue(max_queue_length, destroy_job);
}
// Check the status of background jobs
void check_jobs_bg_status() 
{
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.

  size_t len = length_jobs_queue(&jobs_queue_g);
  for (size_t i = 0; i < len; i++)
  {
    Job job = pop_front_jobs_queue(&jobs_queue_g);
    bool complete = true;

    for (int j = 0; j < job.num_pids; ++j)
    {
      pid_t pid = job.pids[j];
      
      if (waitpid(pid, NULL, WNOHANG) == 0)
      {
        complete = false;
        break;
      }
    }

    if (complete)
    {
      job.job_complete = true;
      //job.cmd_str = get_command_string();
      print_job_bg_complete(job.job_id, job.pids[0], job.cmd_str);
    } else {
      push_back_jobs_queue(&jobs_queue_g, job);
    }
  }
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) 
{
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) 
{
  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) 
{
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) 
{
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
  char* exec = cmd.args[0];
  char** args = cmd.args;

  execvp(exec, args);

  perror("ERROR: Failed to execute program");
  exit(1);
}

// Print strings
void run_echo(EchoCommand cmd) 
{
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char** strs = cmd.args;
  for (int i = 0; strs[i] != NULL; ++i)
  {
    printf("%s", strs[i]);
    if (strs[i + 1] != NULL)
    {
      printf(" ");
    }
  }
  printf("\n");

  // Flush the buffer before returning
  fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd) 
{
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd) 
{
  // Get the directory name
  const char* dir = cmd.dir;

  // Check if the directory is valid
  if (dir == NULL) 
  {
    perror("ERROR: Failed to resolve path");
    return;
  }


  struct ExportCommand old_cmd = {
    CD,
    strdup("OLDPWD"),
    strdup(lookup_env("PWD"))
  };  
  run_export(old_cmd);

  chdir(dir);

  struct ExportCommand new_cmd = {
    CD,
    strdup("PWD"),
    strdup(dir)
  };
  run_export(new_cmd);
  free(old_cmd.env_var);
  free(old_cmd.val);
  free(new_cmd.env_var);
  free(new_cmd.val);
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) 
{
  int signal = cmd.sig;
  int job_id = cmd.job;

  size_t len = length_jobs_queue(&jobs_queue_g);
  
  for (size_t i = 0; i < len; ++i)
  {
    Job job = pop_front_jobs_queue(&jobs_queue_g);

    if (job.job_id == job_id)
    {
      for (int j = 0; j < job.num_pids; ++j)
      {
        kill(job.pids[j], signal);
      }
    }

    push_back_jobs_queue(&jobs_queue_g, job);
  }
}


// Prints the current working directory to stdout
void run_pwd() 
{
  bool should_free;
  char* cwd = get_current_directory(&should_free);
  assert(cwd != NULL);
  // Print the current working directory
  fprintf(stdout, "%s\n", cwd);
  // Flush the buffer before returning
  fflush(stdout);
  if (should_free) 
  {
    free(cwd);
  }
}
// Prints all background jobs currently in the job list to stdout
void run_jobs() 
{
  size_t len = length_jobs_queue(&jobs_queue_g);
  for (size_t i = 0; i < len; ++i)
  {
    Job job = peek_front_jobs_queue(&jobs_queue_g);
    if ((job.num_pids == 0) || job.pids == NULL)
    {
      fprintf(stderr, "Warning: job %d has no processes\n", job.job_id);
      continue;
    }
    const char* cmd = job.cmd_str ? job.cmd_str : "<unknown>";
    print_job(job.job_id, job.pids[0], cmd);
  }

  // Flush the buffer before returning
  fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some wayIMPLEMENT ME: src/execute.c(line 62): check_jobs_bg_status()
/opt/pycharm-community-2024.2.1/bin/:/opt/pycharm-community-2024.2.1/bin/:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/home/j117p402/bin
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */


/* HINT
The create_process() function is intended to be the place where you fork processes, handle pipe creation, and file redirection. 
You should not call execvp(3) from this function. Instead you should call derivatives of the example_run_command() function. 
Also you can determine whether you should use the boolean variables at the top of this function to determine if pipes and redirects should be setup.
It may be necessary to keep a global execution state structure so that different calls to create process can view important information created in 
previous invocations of create_process() (i.e. the file descriptors for open pipes of previous processes).
*/

typedef struct exec_state_s
{
  int cur_pipe[2];
  int prev_pipe_read; 
} exec_state_t;

exec_state_t exec_g;

void exec_state_init()
{
  exec_g.prev_pipe_read = -1;
  return;
}


pid_t create_process(CommandHolder holder) 
{
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true
  

  int p[2];
  if (p_out)
  {
    pipe(p);
  }

  // 

  pid_t pid = fork();
  // Child process
  if (pid == 0) 
  {
    if (p_in) 
    {
      dup2(exec_g.prev_pipe_read, STDIN_FILENO);
      close(exec_g.prev_pipe_read);
    }
    if (p_out)
    {
      dup2(p[1], STDOUT_FILENO);
      close(p[0]);
      close(p[1]);
    }
    
    if (r_in)
    {
      int fd = open(holder.redirect_in, O_RDONLY);
      dup2(fd, STDIN_FILENO);
      close(fd);
    }

    if (r_out)
    {
      if (r_app)
      {
        // 0644 is the octal representation of the permissions where read = 4, write = 2, execute = 1;
        // Owner = read + write = 6, Group = read = 4, Other = read = 4
        int fd = open(holder.redirect_out, O_APPEND | O_WRONLY | O_CREAT, 0644);
        dup2(fd, STDOUT_FILENO);
        close(fd);
      } else {
        int fd = open(holder.redirect_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }
    }




    child_run_command(holder.cmd);
    exit(0);


  // Parent Process
  } else { 
    
    if (p_in)
    {
      close(exec_g.prev_pipe_read);
    }

    if (p_out)
    {
      close(p[1]);
      exec_g.prev_pipe_read = p[0];
    }

    parent_run_command(holder.cmd);
    return pid;
  }
  // Track PID
}

int get_next_job_id() 
{
  int max_id = 0;
  size_t len = length_jobs_queue(&jobs_queue_g);

  for (size_t i = 0; i < len; ++i)
  {
    Job job = pop_front_jobs_queue(&jobs_queue_g);
    if (job.job_id > max_id)
    {
      max_id = job.job_id;
    }
    push_back_jobs_queue(&jobs_queue_g, job);
  }

  return max_id + 1;
}


// Run a list of commands
void run_script(CommandHolder* holders) 
{
  if (holders == NULL) return;
  exec_state_init();
  check_jobs_bg_status();

  if (get_command_holder_type(holders[0]) == EXIT && get_command_holder_type(holders[1]) == EOC) 
  {
    end_main_loop();
    return;
  }
  // BUG: jobs will execute, but then has a chance of looping syntax errors forever for some reason
  // This specifically happens with sleep 15
  
  pid_t* bg_pids = NULL;
  size_t num_pids = 0;
  size_t pid_cap = 0;


  CommandType type;

  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
  {
    pid_t pid = create_process(holders[i]);
    
    if (holders[0].flags & BACKGROUND)
    {
      if (num_pids >= pid_cap)
      {
        pid_cap = pid_cap == 0 ? 4 : pid_cap * 2;
        pid_t* new_pids = realloc(bg_pids, pid_cap * sizeof(pid_t));
        if (!new_pids)
        {
          perror("ERROR: realloc failed for bg_pids");
          free(bg_pids);
          exit(1);
        }
        bg_pids = new_pids;
      }
    
      bg_pids[num_pids] = pid;
      num_pids++;
    }
  }

  if (exec_g.prev_pipe_read != -1)
  {
    close(exec_g.prev_pipe_read);
    exec_g.prev_pipe_read = -1;
  }

  if (!(holders[0].flags & BACKGROUND)) 
  {
    int status;
    while(wait(&status) > 0);
  } else {

    Job job;

    job.job_id = get_next_job_id();
    job.num_pids = num_pids;

    job.pids = malloc(sizeof(pid_t) * job.num_pids);
    memcpy(job.pids, bg_pids, sizeof(pid_t) * num_pids);

    job.cmd_str = strdup(get_command_string());
    job.job_complete = false;

    push_back_jobs_queue(&jobs_queue_g, job);
    print_job_bg_start(job.job_id, job.pids[0], job.cmd_str);

    free(bg_pids);
  }
}
