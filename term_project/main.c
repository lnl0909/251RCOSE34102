#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

#define MAX_PROCESS 100
#define MAX_QUEUE_SIZE 100
#define TIME_QUANTUM 4
#define IDLE_PID 0

typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

typedef struct {
    int pid;
    int arrival_time;
    int cpu_burst_time;
    int original_cpu_burst_time;
    int remaining_time;
    int io_request_time;
    int io_burst_time;
    int priority;
    int waiting_time;
    int turnaround_time;
    int start_time;
    int completion_time;
    int io_start_time;
    int io_remaining_time;
    ProcessState state;
    bool io_requested_this_burst;
    int current_cpu_burst_done;
} Process;

typedef struct {
    Process* data[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
} Queue;

typedef struct {
    Process processes[MAX_PROCESS];
    int process_count;
    Queue ready_queue;
    Queue waiting_queue;
    int current_time;
    int completed_process_count;
    Process* running_process;
    int total_cpu_time_for_gantt;
} System;

typedef struct {
    char algorithm_name[50];
    double avg_waiting_time;
    double avg_turnaround_time;
    int* gantt_chart;
    int gantt_size;
} SchedulingResult;

void init_queue(Queue* queue) {
    queue->front = 0;
    queue->rear = -1;
    queue->count = 0;
}

bool is_empty(Queue* queue) {
    return queue->count == 0;
}

bool is_full(Queue* queue) {
    return queue->count == MAX_QUEUE_SIZE;
}

void enqueue(Queue* queue, Process* process) {
    if (is_full(queue)) return;
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->data[queue->rear] = process;
    queue->count++;
}

Process* dequeue(Queue* queue) {
    if (is_empty(queue)) return NULL;
    Process* process = queue->data[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->count--;
    return process;
}

// 특정한 프로세스 제거를 위해
Process* dequeue_specific(Queue* queue, Process* target_process) {
    if (is_empty(queue) || target_process == NULL) return NULL;
    int current_idx = queue->front;
    for (int i = 0; i < queue->count; i++) {
        if (queue->data[current_idx] == target_process) {
            Process* found_process = queue->data[current_idx];
            int temp_idx = current_idx;
            for (int j = 0; j < queue->count - 1 - i; j++) {
                 int next_idx = (temp_idx + 1) % MAX_QUEUE_SIZE;
                 if (queue->front == queue->rear && queue->count == 1) { // 항목이 하나면
                     break;
                 }
            }
            Process* found_p = NULL;
            int found_at = -1;
            for(int k=0; k < queue->count; k++) {
                int idx = (queue->front + k) % MAX_QUEUE_SIZE;
                if(queue->data[idx] == target_process) {
                    found_p = queue->data[idx];
                    found_at = k;
                    break;
                }
            }
            if(found_p) {
                for(int k = found_at; k < queue->count - 1; k++) {
                    int current_physical_idx = (queue->front + k) % MAX_QUEUE_SIZE;
                    int next_physical_idx = (queue->front + k + 1) % MAX_QUEUE_SIZE;
                    queue->data[current_physical_idx] = queue->data[next_physical_idx];
                }
                queue->rear = (queue->rear - 1 + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
                queue->count--;
                return found_p;
            }
            return NULL;
        }
        current_idx = (current_idx + 1) % MAX_QUEUE_SIZE; 
    }
    return NULL;
}


void reset_process_state_for_simulation(Process* p, const Process* original_p) {
    p->pid = original_p->pid;
    p->arrival_time = original_p->arrival_time;
    p->cpu_burst_time = original_p->cpu_burst_time;
    p->original_cpu_burst_time = original_p->original_cpu_burst_time;
    p->remaining_time = original_p->cpu_burst_time;
    p->io_request_time = original_p->io_request_time;
    p->io_burst_time = original_p->io_burst_time;
    p->priority = original_p->priority;
    p->waiting_time = 0;
    p->turnaround_time = 0;
    p->start_time = -1;
    p->completion_time = 0;
    p->io_start_time = 0;
    p->io_remaining_time = original_p->io_burst_time;
    p->state = NEW;
    p->io_requested_this_burst = false;
    p->current_cpu_burst_done = 0;
}

// system 구조체 초기화
void config_system_for_simulation(System* sys, const Process original_processes[], int count) {
    sys->process_count = count;
    sys->total_cpu_time_for_gantt = 0;
    for (int i = 0; i < count; i++) {
        reset_process_state_for_simulation(&sys->processes[i], &original_processes[i]);
        sys->total_cpu_time_for_gantt += sys->processes[i].original_cpu_burst_time + sys->processes[i].io_burst_time;
    }
    init_queue(&sys->ready_queue);
    init_queue(&sys->waiting_queue);
    sys->current_time = 0;
    sys->completed_process_count = 0;
    sys->running_process = NULL;
}

void create_initial_processes(Process processes[], int count) {
    srand(time(NULL));
    for (int i = 0; i < count; i++) {
        Process* p = &processes[i];
        p->pid = i + 1;
        p->arrival_time = rand() % 20;
        p->original_cpu_burst_time = (rand() % 10) + 1;
        p->io_request_time = (rand() % p->original_cpu_burst_time);
        if (p->io_request_time == 0 && p->original_cpu_burst_time > 1) p->io_request_time = 1;
        if (p->original_cpu_burst_time == 1) p->io_request_time = 0;
        if (p->io_request_time > 0) {
             p->cpu_burst_time = p->io_request_time;
        } else {
            p->cpu_burst_time = p->original_cpu_burst_time;
        }
        p->remaining_time = p->cpu_burst_time;
        p->io_burst_time = (p->io_request_time > 0) ? (rand() % 5) + 1 : 0;
        p->io_remaining_time = p->io_burst_time;
        p->priority = (rand() % 10) + 1;
        p->waiting_time = 0;
        p->turnaround_time = 0;
        p->start_time = -1;
        p->completion_time = 0;
        p->io_start_time = 0;
        p->state = NEW;
        p->io_requested_this_burst = false;
        p->current_cpu_burst_done = 0;
    }
}

void process_arrivals(System* sys) {
    for (int i = 0; i < sys->process_count; i++) {
        if (sys->processes[i].state == NEW && sys->processes[i].arrival_time <= sys->current_time) {
            sys->processes[i].state = READY;
            enqueue(&sys->ready_queue, &sys->processes[i]);
        }
    }
}

void process_io_completions(System* sys) {
    if (is_empty(&sys->waiting_queue)) return;
    int initial_waiting_count = sys->waiting_queue.count;
    for (int i = 0; i < initial_waiting_count; i++) {
        Process* p_wait = dequeue(&sys->waiting_queue);
        if (p_wait->state == WAITING) {
            p_wait->io_remaining_time--; // io 작업시간 감소
            if (p_wait->io_remaining_time <= 0) {
                p_wait->state = READY;
                p_wait->remaining_time = p_wait->original_cpu_burst_time - p_wait->io_request_time; // 남은 cpu bust 시간
                p_wait->cpu_burst_time = p_wait->remaining_time;
                p_wait->current_cpu_burst_done = 0;
                p_wait->io_requested_this_burst = true;
                enqueue(&sys->ready_queue, p_wait);
            } else {
                enqueue(&sys->waiting_queue, p_wait);
            }
        } else {
             enqueue(&sys->waiting_queue, p_wait);
        }
    }
}

// I/O 요청이 필요한지 확인 + 필요한 경우 프로세스를 Waiting 으로
void handle_io_request_if_needed(Process* p, System* sys) {
    if (p == NULL || p->state != RUNNING) return;
    if (!p->io_requested_this_burst && p->io_request_time > 0 && p->current_cpu_burst_done == p->io_request_time) {
        p->state = WAITING;
        p->io_start_time = sys->current_time;
        p->io_remaining_time = p->io_burst_time;
        enqueue(&sys->waiting_queue, p);
        sys->running_process = NULL;
    }
}

// SJF, Priority 
Process* find_shortest_job_in_queue(Queue* queue) {
    if (is_empty(queue)) return NULL;
    Process* shortest_job = queue->data[queue->front];
    int shortest_time = shortest_job->remaining_time;
    int current_q_idx = (queue->front + 1) % MAX_QUEUE_SIZE; // 이미 첫번째는 shortest-job으로 가정
    for (int i = 1; i < queue->count; i++) {
        Process* p = queue->data[current_q_idx];
        if (p->remaining_time < shortest_time) {
            shortest_time = p->remaining_time;
            shortest_job = p;
        }
        current_q_idx = (current_q_idx + 1) % MAX_QUEUE_SIZE;
    }
    return shortest_job;
}

// priority
Process* find_highest_priority_job_in_queue(Queue* queue) {
    if (is_empty(queue)) return NULL;
    Process* highest_priority_job = queue->data[queue->front];
    int highest_priority = highest_priority_job->priority; // 낮은 숫자가 높은 우선순위
    int current_q_idx = (queue->front + 1) % MAX_QUEUE_SIZE;
    for (int i = 1; i < queue->count; i++) {
        Process* p = queue->data[current_q_idx];
        if (p->priority < highest_priority) {
            highest_priority = p->priority;
            highest_priority_job = p;
        }
        current_q_idx = (current_q_idx + 1) % MAX_QUEUE_SIZE;
    }
    return highest_priority_job;
}

void execute_cpu_cycle(System* sys, int* gantt_chart, int* gantt_size, bool is_rr, int* current_quantum_slice) {
    if (sys->running_process != NULL) {
        gantt_chart[(*gantt_size)++] = sys->running_process->pid;
        sys->running_process->remaining_time--;
        sys->running_process->current_cpu_burst_done++;
        if (is_rr) (*current_quantum_slice)++;

        handle_io_request_if_needed(sys->running_process, sys); // io check(io 요청으로 running process가 null이 될수도)

        if (sys->running_process != NULL) { // io 요청후에도 실행중인지 확인
            if (sys->running_process->remaining_time <= 0) { // 프로세스 완료
                sys->running_process->state = TERMINATED;
                sys->running_process->completion_time = sys->current_time + 1;
                sys->running_process->turnaround_time = sys->running_process->completion_time - sys->running_process->arrival_time;
                sys->running_process->waiting_time = sys->running_process->turnaround_time - sys->running_process->original_cpu_burst_time;
                sys->completed_process_count++;
                sys->running_process = NULL;
                if (is_rr) *current_quantum_slice = 0;
            } else if (is_rr && *current_quantum_slice >= TIME_QUANTUM) { // RR만 따로
                sys->running_process->state = READY;
                enqueue(&sys->ready_queue, sys->running_process);
                sys->running_process = NULL;
                *current_quantum_slice = 0;
            }
        } else { // io 때문에 null 되면
             if (is_rr) *current_quantum_slice = 0;
        }
    } else {
        gantt_chart[(*gantt_size)++] = IDLE_PID;
    }
}

void finalize_scheduling_result(System* sys, SchedulingResult* result, const char* algo_name, int* gantt_chart, int gantt_size) {
    strcpy(result->algorithm_name, algo_name);
    double total_waiting_time = 0;
    double total_turnaround_time = 0;
    for (int i = 0; i < sys->process_count; i++) {
        total_waiting_time += sys->processes[i].waiting_time;
        total_turnaround_time += sys->processes[i].turnaround_time;
    }
    result->avg_waiting_time = (sys->process_count > 0) ? total_waiting_time / sys->process_count : 0;
    result->avg_turnaround_time = (sys->process_count > 0) ? total_turnaround_time / sys->process_count : 0;
    result->gantt_chart = gantt_chart;
    result->gantt_size = gantt_size;
}

SchedulingResult fcfs_scheduling(const Process original_processes[], int count) {
    System sys;
    config_system_for_simulation(&sys, original_processes, count);
    int* gantt_chart = (int*)malloc(sizeof(int) * (sys.total_cpu_time_for_gantt + count * 20 + 100));
    int gantt_size = 0;

    while (sys.completed_process_count < sys.process_count) {
        process_arrivals(&sys);
        process_io_completions(&sys);

        if (sys.running_process == NULL && !is_empty(&sys.ready_queue)) {
            sys.running_process = dequeue(&sys.ready_queue);
            sys.running_process->state = RUNNING;
            if (sys.running_process->start_time == -1) {
                sys.running_process->start_time = sys.current_time;
            }
        }

        execute_cpu_cycle(&sys, gantt_chart, &gantt_size, false, NULL);
        sys.current_time++;
        if (gantt_size >= (sys.total_cpu_time_for_gantt + count * 20 + 90)) break;
    }

    SchedulingResult result;
    finalize_scheduling_result(&sys, &result, "FCFS", gantt_chart, gantt_size);
    return result;
}

SchedulingResult sjf_np_scheduling(const Process original_processes[], int count) {
    System sys;
    config_system_for_simulation(&sys, original_processes, count);
    int* gantt_chart = (int*)malloc(sizeof(int) * (sys.total_cpu_time_for_gantt + count * 20 + 100));
    int gantt_size = 0;

    while (sys.completed_process_count < sys.process_count) {
        process_arrivals(&sys);
        process_io_completions(&sys);

        if (sys.running_process == NULL && !is_empty(&sys.ready_queue)) {
            Process* shortest_job = find_shortest_job_in_queue(&sys.ready_queue);
            if (shortest_job != NULL) {
                sys.running_process = dequeue_specific(&sys.ready_queue, shortest_job);
                if (sys.running_process) {
                    sys.running_process->state = RUNNING;
                    if (sys.running_process->start_time == -1) {
                        sys.running_process->start_time = sys.current_time;
                    }
                }
            }
        }

        execute_cpu_cycle(&sys, gantt_chart, &gantt_size, false, NULL);
        sys.current_time++;
        if (gantt_size >= (sys.total_cpu_time_for_gantt + count * 20 + 90)) break;
    }

    SchedulingResult result;
    finalize_scheduling_result(&sys, &result, "Non-Preemptive SJF", gantt_chart, gantt_size);
    return result;
}

SchedulingResult priority_np_scheduling(const Process original_processes[], int count) {
    System sys;
    config_system_for_simulation(&sys, original_processes, count);
    int* gantt_chart = (int*)malloc(sizeof(int) * (sys.total_cpu_time_for_gantt + count * 20 + 100));
    int gantt_size = 0;

    while (sys.completed_process_count < sys.process_count) {
        process_arrivals(&sys);
        process_io_completions(&sys);

        if (sys.running_process == NULL && !is_empty(&sys.ready_queue)) {
            Process* highest_priority_job = find_highest_priority_job_in_queue(&sys.ready_queue);
            if (highest_priority_job != NULL) {
                sys.running_process = dequeue_specific(&sys.ready_queue, highest_priority_job);
                if(sys.running_process){
                    sys.running_process->state = RUNNING;
                    if (sys.running_process->start_time == -1) {
                        sys.running_process->start_time = sys.current_time;
                    }
                }
            }
        }
        
        execute_cpu_cycle(&sys, gantt_chart, &gantt_size, false, NULL);
        sys.current_time++;
        if (gantt_size >= (sys.total_cpu_time_for_gantt + count * 20 + 90)) break;
    }

    SchedulingResult result;
    finalize_scheduling_result(&sys, &result, "Non-Preemptive Priority", gantt_chart, gantt_size);
    return result;
}

SchedulingResult rr_scheduling(const Process original_processes[], int count) {
    System sys;
    config_system_for_simulation(&sys, original_processes, count);
    int* gantt_chart = (int*)malloc(sizeof(int) * (sys.total_cpu_time_for_gantt + count * 20 + 100));
    int gantt_size = 0;
    int current_quantum_slice = 0;

    while (sys.completed_process_count < sys.process_count) {
        process_arrivals(&sys);
        process_io_completions(&sys);

        if (sys.running_process == NULL && !is_empty(&sys.ready_queue)) {
            sys.running_process = dequeue(&sys.ready_queue);
            sys.running_process->state = RUNNING;
            if (sys.running_process->start_time == -1) {
                sys.running_process->start_time = sys.current_time;
            }
            current_quantum_slice = 0; 
        }

        execute_cpu_cycle(&sys, gantt_chart, &gantt_size, true, &current_quantum_slice);
        sys.current_time++;
        if (gantt_size >= (sys.total_cpu_time_for_gantt + count * 20 + 90)) break;
    }

    SchedulingResult result;
    finalize_scheduling_result(&sys, &result, "Round Robin", gantt_chart, gantt_size);
    return result;
}

SchedulingResult sjf_p_scheduling(const Process original_processes[], int count) {
    System sys;
    config_system_for_simulation(&sys, original_processes, count);
    int* gantt_chart = (int*)malloc(sizeof(int) * (sys.total_cpu_time_for_gantt + count * 20 + 100));
    int gantt_size = 0;

    while (sys.completed_process_count < sys.process_count) {
        process_arrivals(&sys);
        process_io_completions(&sys);

        Process* candidate_in_ready_q = find_shortest_job_in_queue(&sys.ready_queue);
        
        bool preempt = false;
        if (sys.running_process != NULL && candidate_in_ready_q != NULL) {
            if (candidate_in_ready_q->remaining_time < sys.running_process->remaining_time) {
                preempt = true;
            }
        } else if (sys.running_process == NULL && candidate_in_ready_q != NULL) {
            preempt = true; 
        }

        if (preempt) {
            if (sys.running_process != NULL) {
                sys.running_process->state = READY;
                enqueue(&sys.ready_queue, sys.running_process);
            }
            sys.running_process = dequeue_specific(&sys.ready_queue, candidate_in_ready_q);
            if(sys.running_process){
                sys.running_process->state = RUNNING;
                if (sys.running_process->start_time == -1) {
                    sys.running_process->start_time = sys.current_time;
                }
            }
        }
        
        execute_cpu_cycle(&sys, gantt_chart, &gantt_size, false, NULL);
        sys.current_time++;
        if (gantt_size >= (sys.total_cpu_time_for_gantt + count * 20 + 90)) break;
    }

    SchedulingResult result;
    finalize_scheduling_result(&sys, &result, "Preemptive SJF", gantt_chart, gantt_size);
    return result;
}

SchedulingResult priority_p_scheduling(const Process original_processes[], int count) {
    System sys;
    config_system_for_simulation(&sys, original_processes, count);
    int* gantt_chart = (int*)malloc(sizeof(int) * (sys.total_cpu_time_for_gantt + count * 20 + 100));
    int gantt_size = 0;

    while (sys.completed_process_count < sys.process_count) {
        process_arrivals(&sys);
        process_io_completions(&sys);

        Process* candidate_in_ready_q = find_highest_priority_job_in_queue(&sys.ready_queue);

        bool preempt = false;
        if (sys.running_process != NULL && candidate_in_ready_q != NULL) {
            if (candidate_in_ready_q->priority < sys.running_process->priority) {
                preempt = true;
            }
        } else if (sys.running_process == NULL && candidate_in_ready_q != NULL) {
            preempt = true;
        }

        if (preempt) {
            if (sys.running_process != NULL) {
                sys.running_process->state = READY;
                enqueue(&sys.ready_queue, sys.running_process);
            }
            sys.running_process = dequeue_specific(&sys.ready_queue, candidate_in_ready_q);
            if(sys.running_process){
                sys.running_process->state = RUNNING;
                if (sys.running_process->start_time == -1) {
                    sys.running_process->start_time = sys.current_time;
                }
            }
        }

        execute_cpu_cycle(&sys, gantt_chart, &gantt_size, false, NULL);
        sys.current_time++;
        if (gantt_size >= (sys.total_cpu_time_for_gantt + count * 20 + 90)) break;
    }

    SchedulingResult result;
    finalize_scheduling_result(&sys, &result, "Preemptive Priority", gantt_chart, gantt_size);
    return result;
}

void print_gantt_chart(SchedulingResult result) {
    printf("\nGantt Chart (%s):\n\n", result.algorithm_name);
    if (result.gantt_size == 0) {
        printf("Gantt chart is empty\n");
        return;
    }
    
    printf(" ");
    for (int i = 0; i < result.gantt_size; i++) {
        printf("%-4d", i);
    }
    printf("\n");

    printf("|");
    for (int i = 0; i < result.gantt_size; i++) {
        if (result.gantt_chart[i] == IDLE_PID) {
            printf("Idel|"); 
        } else {
            printf("P%-2d|", result.gantt_chart[i]); 
        }
    }
    printf("\n");
}

void print_scheduling_result(SchedulingResult result) {
    printf("\n%s Algorithm Results:\n", result.algorithm_name);
    printf("--------------------------------------------------------------------------------------------------\n");
    printf("Average Waiting Time   : %-6.2f\n", result.avg_waiting_time);
    printf("Average Turnaround Time: %-6.2f\n", result.avg_turnaround_time);
    print_gantt_chart(result);
}

Process original_process_list[MAX_PROCESS];

void evaluate_algorithms(int process_count) {
    SchedulingResult fcfs_result = fcfs_scheduling(original_process_list, process_count);
    SchedulingResult sjf_np_result = sjf_np_scheduling(original_process_list, process_count);
    SchedulingResult priority_np_result = priority_np_scheduling(original_process_list, process_count);
    SchedulingResult rr_result = rr_scheduling(original_process_list, process_count);
    SchedulingResult sjf_p_result = sjf_p_scheduling(original_process_list, process_count);
    SchedulingResult priority_p_result = priority_p_scheduling(original_process_list, process_count);
    
    print_scheduling_result(fcfs_result);
    print_scheduling_result(sjf_np_result);
    print_scheduling_result(priority_np_result);
    print_scheduling_result(rr_result);
    print_scheduling_result(sjf_p_result);
    print_scheduling_result(priority_p_result);
    
    free(fcfs_result.gantt_chart);
    free(sjf_np_result.gantt_chart);
    free(priority_np_result.gantt_chart);
    free(rr_result.gantt_chart);
    free(sjf_p_result.gantt_chart);
    free(priority_p_result.gantt_chart);
}

int main() {
    int process_count_input;
    
    printf("The number of processes");
    scanf("%d", &process_count_input);
    
    create_initial_processes(original_process_list, process_count_input);
    
    printf("\nCreated Process Information:\n");
    printf("%-5s %-10s %-10s %-12s %-10s %-8s\n", 
           "PID", "Arrival", "CPU Burst", "I/O Request", "I/O Burst", "Priority");
    printf("-------------------------------------------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < process_count_input; i++) {
        Process p = original_process_list[i];
        printf("%-5d %-10d %-10d %-12d %-10d %-8d\n", 
               p.pid, p.arrival_time, p.original_cpu_burst_time, 
               p.io_request_time, p.io_burst_time, p.priority);
    }
    
    evaluate_algorithms(process_count_input);
    
    return 0;
}