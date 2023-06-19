#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <mqueue.h>

struct person{
    int id;
    char* name;
    char* days;
};

void handler(int signo, siginfo_t *info, void *context) {
    int* noOfSignals = info->si_value.sival_ptr;
    (*noOfSignals)++;
}

struct person* readApplicants(struct person* people, int* numberOfPeople,int currentNoOfPeople[], int dailyLimit[]);
void writeApplicants(struct person* people, int numberOfPeople);
void openMenu(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]);
char* readFromStandardInput();
struct person* addNewPerson(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]);
void modifyPerson(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]);
struct person* deletePerson(struct person* people, int* numberOfPeople, int currentNoOfPeople[]);
void listPeople(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]);
int checkDays (const int dailyLimit[], int currentNoOfPeople[], char* days);
int getNewId(struct person* people, int numberOfPeople);
int switchDayListByToken(int* dayList, char* token);
void revertDays(int currentNoOfPeople[], char* days);
int checkIfStringIsNull(char* string);
void simulation(struct person* people, int numberOfPeople);
int whichDay(char* day);

int main() {
    int dailyLimit[] = {15,15,15,15,15,15,0};
    int currentNoOfPeople[] = {0,0,0,0,0,0,0};
    int numberOfPeople = 0;

    struct person* people = (struct person*) malloc(sizeof(struct person));
    people = readApplicants(people,&numberOfPeople,currentNoOfPeople, dailyLimit);

    openMenu(people, &numberOfPeople, dailyLimit, currentNoOfPeople);

    return 0;
}

struct person* readApplicants(struct person* people, int* numberOfPeople,int currentNoOfPeople[], int dailyLimit[]) {

    int applicants = open("Applicants.txt", O_RDONLY | O_CREAT, 444);
    if(applicants == -1){
        perror("Hiba tortent a fajl megnyitasa soran!\n");
        return people;
    }

    int needToRead = 0;

    int lengthOfFile = lseek(applicants,lseek(applicants,0,SEEK_SET),SEEK_END);
    lseek(applicants,0,SEEK_SET);

    while(lengthOfFile > lseek(applicants,0,SEEK_CUR)){

        read(applicants,  &people[*numberOfPeople].id, sizeof(int));

        read(applicants, &needToRead, sizeof(int));
        people[*numberOfPeople].name = malloc(sizeof(char)*needToRead);
        read(applicants, people[*numberOfPeople].name, needToRead * sizeof(char));

        read(applicants, &needToRead, sizeof(int));
        people[*numberOfPeople].days = malloc(sizeof(char)*needToRead);
        read(applicants, people[*numberOfPeople].days, needToRead * sizeof(char));

        checkDays(dailyLimit,currentNoOfPeople,people[*numberOfPeople].days);

        (*numberOfPeople)++;
        people = realloc(people,((*numberOfPeople)+1)*sizeof(struct person));
    }

    close(applicants);

    return people;
}

void writeApplicants(struct person* people, int numberOfPeople){

    int applicants = open("Applicants.txt", O_WRONLY|O_TRUNC,666);
    if(applicants == -1){
        perror("Hiba a fajl iras soran!\n");
        return;
    }

    int length;
    int i = 0;

    while(i < numberOfPeople){

        write(applicants,&people[i].id,sizeof(int));

        length = (int)(strlen(people[i].name)+1);

        write(applicants, &length,sizeof(int));
        write(applicants,people[i].name,length*sizeof(char));

        length = (int)(strlen(people[i].days)+1);

        write(applicants, &length,sizeof(int));
        write(applicants,people[i].days,length*sizeof(char));

        i++;
    }
    close(applicants);
}

void openMenu(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]){

    int command = -1;

    while(command != 0){
        printf("0 - Kilepes\n");
        printf("1 - Uj jelentkezo felvetele\n");
        printf("2 - Jelentkezo modositasa\n");
        printf("3 - Jelentkezo torlese\n");
        printf("4 - Elerhetosegek kilistazasa\n");
        printf("5 - Szimulacio inditasa\n");

        scanf("%d", &command);
        while(getchar()!='\n'){}

        switch (command) {
            case 1:
                people = addNewPerson(people, numberOfPeople, dailyLimit, currentNoOfPeople);
                writeApplicants(people,*numberOfPeople);
                break;
            case 2:
                modifyPerson(people, numberOfPeople, dailyLimit, currentNoOfPeople);
                writeApplicants(people,*numberOfPeople);
                break;
            case 3:
                people = deletePerson(people, numberOfPeople, currentNoOfPeople);
                writeApplicants(people,*numberOfPeople);
                break;
            case 4:
                listPeople(people, numberOfPeople, dailyLimit, currentNoOfPeople);
                break;
            case 5:
                simulation(people, *numberOfPeople);
                break;
            default:
                break;
        }
        if(command != 0) command = -1;
    }
}

void simulation(struct person* people, int numberOfPeople){
    printf("Melyik napra kivanja futtatni a szimulaciot?(ekezetek nelkul, csupa kisbetuvel)\n");
    char *when = readFromStandardInput();

    int day = whichDay(when);
    if(day == -1){
        return;
    }

    int noOfSignals = 0;
    pid_t child1, child2 = 1;

    int pipe1[2], pipe2[2];
    if(!(pipe(pipe1) == 0 && pipe(pipe2) == 0)){perror("Hiba a csovek letrehozasa kozben!\n");return;}

    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGUSR1);
    sigaddset(&signals, SIGUSR2);
    sigprocmask(SIG_SETMASK,&signals,NULL);

    char message[2];

    struct mq_attr attributes;
    attributes.mq_maxmsg = 2;
    attributes.mq_msgsize = sizeof(char)*2;

    int messageId = mq_open("/MQ_OPEN", O_CREAT | O_RDWR , 0666, &attributes);
    if(messageId == -1){perror("Hiba az uzenetsor letrehozasa kozben!");return;}

    child1 = fork();
    if(child1 != 0){child2 = fork();}

    if(child1 < 0 || child2 < 0){perror("Hiba a gyerekfolyamat inditas kozben!\n");return;}

    if(child1 != 0 && child2 != 0){

        struct sigaction sa;
        sa.sa_sigaction = handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGUSR1,&sa,NULL);
        sigaction(SIGUSR2,&sa,NULL);

        while(noOfSignals < 2){
            sigsuspend(&sa.sa_mask);
        }

        close(pipe1[0]);
        close(pipe2[0]);

        int count = 0;
        int dayList[] = {0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < numberOfPeople && count < 10; i++) {

            for(int j = 0; j < 6; j++){dayList[j] = 0;}

            char *token = strtok(people[i].days, " ");
            while (token != NULL) {
                if (switchDayListByToken(dayList, token) == 0) return;
                token = strtok(NULL, " ");
            }

            if (dayList[day] == 1 && count < 5) {
                write(pipe1[1], &people[i].id, sizeof(int));
                count++;
            } else if (dayList[day] == 1 && count >= 5) {
                write(pipe2[1], &people[i].id, sizeof(int));
                count++;
            }
        }
        close(pipe1[1]);
        close(pipe2[1]);

        while(noOfSignals < 4){
            sigsuspend(&sa.sa_mask);
        }

        int sum = 0;

        long messageError = mq_receive(messageId, message, sizeof(char)*2, 0);
        if(messageError == -1){
            perror("Hiba az uzenetsor fogadasa kozben!\n");
        }
        sum += message[0]-'0';
        messageError = mq_receive(messageId, message, sizeof(char)*2, 0);
        if(messageError == -1){
            perror("Hiba az uzenetsor fogadasa kozben!\n");
        }
        sum += message[0]-'0';
        printf("A munkasautok osszesen %d embert tudtak behozni!\n", sum);
    }

    if (child1 == 0 && child2 != 0) {
        union sigval value;
        value.sival_ptr = &noOfSignals;
        sigqueue(getppid(), SIGUSR1, value);

        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        int previousId = -1;
        int currentId = -1;
        int sum = 0;
        int i = 0;
        do{
            previousId = currentId;
            read(pipe1[0],&currentId, sizeof(int));
            if(currentId != 0 && currentId != previousId){
                printf("Az 1-es munkasjarat behozta a %d-s id-vel rendelkezo dolgozot!\n",currentId);
                sum++;
            }
            i++;
        }while(previousId != currentId && i < 5);
        close(pipe1[0]);

        message[0] = sum+'0';
        message[1] = '\0';
        mq_send(messageId, message, sizeof(char)*2, 0);
        sigqueue(getppid(), SIGUSR1, value);
    }

    if (child2 == 0 && child1 != 0) {
        union sigval value;
        value.sival_ptr = &noOfSignals;
        sigqueue(getppid(), SIGUSR2, value);

        close(pipe2[1]);
        close(pipe1[0]);
        close(pipe1[1]);

        int previousId = -1;
        int currentId = -1;
        int sum = 0;
        int i = 0;
        do{
            previousId = currentId;
            read(pipe2[0],&currentId, sizeof(int));
            if(currentId != 0 && currentId != previousId){
                printf("Az 2-es munkasjarat behozta a %d-s id-vel rendelkezo dolgozot!\n",currentId);
                sum++;
            }
            i++;
        }while(previousId != currentId && i < 5);
        close(pipe2[0]);

        message[0] = sum+'0';
        message[1] = '\0';
        mq_send(messageId, message, sizeof(char)*2, 0);
        sigqueue(getppid(), SIGUSR2, value);
    }
    if ((child1 == 0 && child2 != 0) || (child2 == 0 && child1 != 0)) exit(0);

    mq_close(messageId);
    mq_unlink("/MQ_OPEN");

    free(when);
}

struct person* addNewPerson(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]){

    printf("Adja meg az uj jelentkezo nevet (ekezetek nelkul): \n");
    char* newName = readFromStandardInput();

    if(checkIfStringIsNull(newName) == 0){
        return people;
    }

    printf("Adja meg mely napokon elerheto %s (ekezetek nelkul, szokozzel elvalasztva, csupa kisbetuvel): \n", newName);
    char* newDays = readFromStandardInput();

    if(checkIfStringIsNull(newDays) == 0){
        return people;
    }

    int isThereEnoughSpace = checkDays(dailyLimit,currentNoOfPeople,newDays);

    if(isThereEnoughSpace == 0){
        return people;
    }else{
        people = realloc(people,(++(*numberOfPeople))*sizeof(struct person));

        people[*numberOfPeople-1].id = getNewId(people,*numberOfPeople);
        people[*numberOfPeople-1].name = newName;
        people[*numberOfPeople-1].days = newDays;
        }
    return people;
}

void modifyPerson(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]){
    int command = 0;

    printf("0 - Vissza\n");
    printf("1 - Jelentkezo nevenek modositasa\n");
    printf("2 - Jelentkezo elerhetosegenek modositasa\n");
    printf("3 - Mindketto modositasa\n");

    scanf("%d", &command);
    while(getchar()!='\n'){}

    if(command == 0) return;

    int isThereEnoughSpace;
    int i = 0;
    int id = 0;
    int didWeFindThePerson = -1;
    int tempCurrentNoOfPeople[7];
    while(i < 7){
        tempCurrentNoOfPeople[i] = currentNoOfPeople[i];
        i++;
    }

    char* newName;
    char* newDays;

    printf("Adja meg a modositando jelentkezo id-et: \n");

    scanf("%d", &id);
    while(getchar()!='\n'){}

    i = 0;
    while(i < *numberOfPeople && didWeFindThePerson == -1){
        if(id == people[i].id){
            didWeFindThePerson = i;
        }
        i++;
    }

    if(didWeFindThePerson == -1){
        printf("Nem letezik ilyen id-vel jelentkezo!\n");
        return;
    }
    id = didWeFindThePerson;

    if(command == 1 ||command == 3){
        printf("Adja meg a jelentkezo uj nevet (ekezetek nelkul): \n");
        newName = readFromStandardInput();

        if(checkIfStringIsNull(newName) == 0){
            return;
        }

        people[id].name = (char*)realloc(newName,(strlen(newName)+1)*sizeof(char));
        people[id].name = newName;
    }

    if(command == 2 ||command == 3){
        printf("Adja meg az uj elerhetosegenek napjat/napjait (csupa kisbetuvel, szokozzel elvalasztva, ekezetek nelkul): \n");
        newDays = readFromStandardInput();

        if(checkIfStringIsNull(newDays) == 0){
            return;
        }

        revertDays(tempCurrentNoOfPeople,people[id].days);

        isThereEnoughSpace = checkDays(dailyLimit,tempCurrentNoOfPeople,newDays);

        if(isThereEnoughSpace != 0){
            people[id].days = (char*)realloc(newDays,(strlen(newDays)+1)*sizeof(char));
            people[id].days = newDays;

            i = 0;
            while(i < 7){
                currentNoOfPeople[i] = tempCurrentNoOfPeople[i];
                i++;
            }
        }
    }
}

struct person* deletePerson(struct person* people, int* numberOfPeople, int currentNoOfPeople[]){
    int i = 0;
    int id = 0;
    int didWeFindThePerson = -1;

    printf("Adja meg a torlendo jelentkezo id-et: \n");

    scanf("%d", &id);
    while(getchar()!='\n'){}

    while(i < *numberOfPeople && didWeFindThePerson == -1){
        if(id == people[i].id){
            didWeFindThePerson = i;
        }
        i++;
    }

    if(didWeFindThePerson == -1){
        printf("Nem letezik ilyen id-vel jelentkezo!\n");
        return people;
    }

    revertDays(currentNoOfPeople,people[didWeFindThePerson].days);

    struct person* tempPeople = (struct person*) malloc(sizeof(struct person)*(--(*numberOfPeople)));

    i = 0;
    int j = 0;
    while(i < *numberOfPeople+1){
        if(people[i].id != id){
            tempPeople[j] = people[i];
            j++;
        }
        i++;
    }

    free(people);
    return tempPeople;
}

void listPeople(struct person* people, int* numberOfPeople, int dailyLimit[], int currentNoOfPeople[]){

    int i = 0;
    printf("Szukseges emberek szama minden napra hetfotol vasarnapig sorrendben: ");
    while(i < 7){
        printf("%d ",dailyLimit[i]-currentNoOfPeople[i]);
        i++;
    }
    i = 0;

    printf("\n-\n");
    while(i < *numberOfPeople){
        printf("Id: %d\n",people[i].id);
        printf("Nev: %s\n",people[i].name);
        printf("Elerheto napok: %s\n",people[i].days);
        printf("-\n");

        i++;
    }


}

char* readFromStandardInput() {
    int i = 0;
    char c;
    char *string = (char*) malloc(1 * sizeof(char));
    int size = 1;

    while ((c = getchar()) != '\n') {
        string[i] = c;
        string = (char*) realloc(string, (++size) * sizeof(char));

        i++;
    }
    string[i] = '\0';

    return string;
}

int checkDays(const int dailyLimit[], int currentNoOfPeople[], char* days){
    int i = 0;
    int dayList[] = {0,0,0,0,0,0,0};

    char* newDays = (char*)malloc((strlen(days)+1)*sizeof(char));

    strcpy(newDays,days);

    char* token = strtok(newDays, " ");

    while(token != NULL ) {
        if(switchDayListByToken(dayList, token) == 0) return 0;
        token = strtok(NULL, " ");
    }

    free(newDays);

    while(i < 7){
        if(dailyLimit[i] < currentNoOfPeople[i]+dayList[i]){
            printf("Nincs eleg hely a het %d. napjan!\n", i+1);
            return 0;
        }
        i++;
    }

    i = 0;
    while(i < 7){
        currentNoOfPeople[i]+=dayList[i];
        i++;
    }
    return 1;
}

void revertDays(int currentNoOfPeople[], char* days){
    int i = 0;
    int dayList[] = {0,0,0,0,0,0,0};

    char* newDays = (char*)malloc((strlen(days)+1)*sizeof(char));

    strcpy(newDays,days);

    char* token = strtok(newDays, " ");

    while(token != NULL ) {
        if(switchDayListByToken(dayList, token) == 0) return;
        token = strtok(NULL, " ");
    }

    free(newDays);

    i = 0;
    while(i < 7){
        currentNoOfPeople[i]-=dayList[i];
        i++;
    }
}

int getNewId(struct person* people, int numberOfPeople){
    int max = 1;

    int i = 0;
    while(i < numberOfPeople-1){
        if(people[i].id >= max){
            max = people[i].id+1;
        }
        i++;
    }
    return max;
}

int switchDayListByToken(int* dayList, char* token){
    if(strcmp(token,"hetfo") == 0){
        dayList[0] = 1;
    }else if(strcmp(token,"kedd") == 0){
        dayList[1] = 1;
    }else if(strcmp(token,"szerda") == 0){
        dayList[2] = 1;
    }else if(strcmp(token,"csutortok") == 0){
        dayList[3] = 1;
    }else if(strcmp(token,"pentek") == 0){
        dayList[4] = 1;
    }else if(strcmp(token,"szombat") == 0){
        dayList[5] = 1;
    }else if(strcmp(token,"vasarnap") == 0){
        dayList[6] = 1;
    }else if(token != NULL){
        printf("Helytelenul beirt nap!\n");
        return 0;
    }
    return 1;
}

int whichDay(char* day){
    if(strcmp(day,"hetfo") == 0){
        return 0;
    }else if(strcmp(day,"kedd") == 0){
        return 1;
    }else if(strcmp(day,"szerda") == 0){
        return 2;
    }else if(strcmp(day,"csutortok") == 0){
        return 3;
    }else if(strcmp(day,"pentek") == 0){
        return 4;
    }else if(strcmp(day,"szombat") == 0){
        return 5;
    }else if(strcmp(day,"vasarnap") == 0){
        return 6;
    }
    printf("Helytelenul beirt nap!\n");
    return -1;
}

int checkIfStringIsNull(char* string){
    if(strcmp(string,"\0") == 0 ||strcmp(string," ") == 0 ||strcmp(string,"\n") == 0 || strlen(string) < 2){
        printf("Ures vagy tul rovid szoveg (kevesebb mint 2 karakter)!\n");
        return 0;
    }
    return 1;
}