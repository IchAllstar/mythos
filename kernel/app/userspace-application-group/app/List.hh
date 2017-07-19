#pragma once

#include "app/Mutex.hh"

struct ListElement {
	ListElement *next {nullptr};
};

// Simple list
// note that the list does not care what it contains, as long is it is a list element
class List {
public:
	void push(ListElement *elem) {
		mutex << [&]() {
			if (head == nullptr) {
				head = elem;
				return;
			}
			ListElement *tmp = head;
			while (tmp->next != nullptr) {
				tmp = tmp->next;
			}
			tmp->next = elem;
		};
	}

	ListElement* first() {
		mutex << [&]() {
			if (head == nullptr) {
				return nullptr;
			}
			ListElement *tmp = head;
			head = head->next;
			return tmp;
		}
	}

private:
	SpinMutex mutex;
	ListElement *head {nullptr};
};