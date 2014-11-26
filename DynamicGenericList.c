#include <stdio.h>                  // INCLUDES
#include <malloc.h>

typedef struct Element{             // GLOBAL TYPES DEFINITION
    void* value;
    struct Element* next; 
} Node;

Node* addInFront(Node* , void*);      // FUNCTIONS PROTOTYPE
void addInQueue(Node* , void*);
void printList(Node*);
Node* search(Node* , void*);

int main (){
    
    Node* list=NULL;
    Node node;
    
    list=&node;
    
    node.value=(void*)malloc(sizeof(void*));
    int a=5;
    node.value=&a;
    node.next=NULL;
    
    list=addInFront(list, 4);
    list=addInFront(list, 3);
    list=addInFront(list, 2);
    list=addInFront(list, 1);
    addInQueue(list, 6);
    addInQueue(list, 7);
    addInQueue(list, 8);
    addInQueue(list, 9);
    
    printList(list);
    printf("\n");
    
    Node* a=search(list, 6);
    if (a!=NULL)
        printf("%d", a->value);
    
    return 0;
}


Node* addInFront(Node* head, void* val){
    Node* n=(Node*)malloc(sizeof(Node));
    n->value=val;
    n->next=head;
    return n;
     }
     
     
void addInQueue(Node* head, void* val){
    Node* it=head;
    
    while(it->next!=NULL)
        it=it->next;
    
    it->next=(Node*)malloc(sizeof(Node));
    it=it->next;
    it->value=val;
    it->next=NULL;
    }
     
void printList(Node* head){
    printf("list");
    while(head!=NULL){
        printf(" -> %a", *(head->value));
        head=head->next;
    }
}

Node* search(Node* head, void* val){
    while (head!=NULL){
        if(*(head->value)==*val)
            return head;
        head=head->next;
    }
    return NULL;
}



