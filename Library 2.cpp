#include <iostream>
#include <string>
#include <vector>
#include <mysqlx/xdevapi.h>

using namespace std;
using namespace mysqlx;

class Student;
class Admin;
class Login {
public:
    Login(string username, string password, Session& session)
        : username(username), password(password), loggedIn(false), session(session) {}

    bool isLoggedIn() { return loggedIn; }

    void login(string username, string password) {
        if (username == this->username && password == this->password) {
            loggedIn = true;
        } else {
            // handle error
        }
    }

    void logout() {
        loggedIn = false;
    }

private:
    string username;
    string password;
    bool loggedIn;
    Session& session;
};

class Book {
public:
    Book(int id, string title, string author, string publication, bool available, Student* reservedBy, Student* borrowedBy, Table& table)
        : id(id), title(title), author(author), publication(publication), available(available), reservedBy(reservedBy), borrowedBy(borrowedBy), table(table) {}

    int getId() { return id; }
    string getTitle() { return title; }
    string getAuthor() { return author; }
    string getPublication() { return publication; }
    bool isAvailable() { return available; }

    void reserve(Student* student);
    void borrow(Student* student);
    void returnBook();

private:
    int id;
    string title;
    string author;
    string publication;
    bool available;
    Student* reservedBy;
    Student* borrowedBy;
    Table& table;
};

class Student {
public:
    Student(int id, string name, string email, string phone, Login& login, Table& table)
        : id(id), name(name), email(email), phone(phone), login(login), table(table) {}

    int getId() { return id; }
    string getName() { return name; }
    string getEmail() { return email; }
    string getPhone() { return phone; }
    vector<Book*> getBorrowedBooks();
    vector<Book*> getReservedBooks();
    vector<double> getFines();

    void borrowBook(Book* book);
    void returnBook(Book* book);
    void reserveBook(Book* book);

private:
    int id;
    string name;
    string email;
    string phone;
    vector<Book*> borrowedBooks;
    vector<Book*> reservedBooks;
    vector<double> fines;
    Login& login;
    Table& table;
};

class Library {
public:
    Library(Session& session) : session(session) {}

    void addBook(Book& book);
    void removeBook(Book& book);
    void addStudent(Student& student);
    void removeStudent(Student& student);
    
    void borrowBook(Student& student, Book& book);
    void returnBook(Student& student, Book& book);
    
    void reserveBook(Student& student, Book& book);
    
    void addFine(Student& student, double fine);
    
    vector<double> viewFines();

private:
    Session& session;
};

class Admin {
public:
    Admin(int id, string name, string email, string phone, Login& login, Table& table)
        : id(id), name(name), email(email), phone(phone), login(login), table(table) {}
  
    int getId() {return id;}
    string getName() {return name;}
    string getEmail() {return email;}
    string getPhone() {return phone;}
  
    void addBook(Library& library, Book& book);
    void removeBook(Library& library, Book& book);
    void removeStudent(Library& library, Student& student);
  
    void addFine(Library& library, Student& student, double fine);
  
    vector<double> viewFines();

private:
    int id;
    string name;
    string email;
    string phone;
    Login& login;
    Table& table;
};

void Book::reserve(Student* student) {
   if (available && !reservedBy && !borrowedBy) {
       reservedBy = student;
       student->reserveBook(this);
       table.update().set("reserved_by", student->getId()).where("id = :id").bind("id", id).execute();
   } else {
       // handle error
   }
}

void Book::borrow(Student* student) {
   if (available && (!reservedBy || reservedBy == student)) {
       available = false;
       borrowedBy = student;
       reservedBy = nullptr;
       student->borrowBook(this);
      table.update().set("available", 0).set("borrowed_by", student->getId()).set("reserved_by", nullptr).where("id = :id").bind("id", id).execute();
   } else {
       // handle error
   }
}

void Book::returnBook() {
   if (!available && borrowedBy) {
       available = true;
       borrowedBy->returnBook(this);
       borrowedBy = nullptr;
       table.update().set("available", 1).set("borrowed_by", nullptr).where("id = :id").bind("id", id).execute();
   } else {
       // handle error
   }
}

vector<Book*> Student::getBorrowedBooks() {
   vector<Book*> result;
   auto res = table.select().where("borrowed_by = :id").bind("id", id).execute();
   for (auto row : res) {
       int book_id = row[0];
       string title = row[1];
       string author = row[2];
       string publication = row[3];
       bool available = row[4];
       Student* reservedBy = nullptr;
       if (!row[5].isNull()) {
           reservedBy = new Student(row[5], "", "", "", login, table);
       }
       Book* book = new Book(book_id, title, author, publication, available, reservedBy, this, table);
       borrowedBooks.push_back(book);
       result.push_back(book);
   }
   return result;
}

vector<Book*> Student::getReservedBooks() {
   vector<Book*> result;
   auto res = table.select().where("reserved_by = :id").bind("id", id).execute();
   for (auto row : res) {
       int book_id = row[0];
       string title = row[1];
       string author = row[2];
       string publication = row[3];
       bool available = row[4];
       Student* reservedBy = nullptr;
       if (!row[5].isNull()) {
           reservedBy = new Student(row[5], "", "", "", login, table);
       }
       Book* book = new Book(book_id, title, author, publication, available, reservedBy, nullptr, table);
       reservedBooks.push_back(book);
       result.push_back(book);
   }
   return result;
}

vector<double> Student::getFines() {
   vector<double> result;
   auto res = table.select("fine").where("student_id = :id").bind("id", id).execute();
   for (auto row : res) {
       double fine = row[0];
       fines.push_back(fine);
       result.push_back(fine);
   }
   return result;
}

void Student::borrowBook(Book* book) {
   borrowedBooks.push_back(book);
   table.insert("borrowed_books").values(id, book->getId()).execute();
}

void Student::returnBook(Book* book) {
   auto it = std::find(borrowedBooks.begin(), borrowedBooks.end(), book);
   if (it != borrowedBooks.end()) {
       borrowedBooks.erase(it);
       table.remove().from("borrowed_books").where("student_id = :student_id and book_id = :book_id").bind("student_id", id).bind("book_id", book->getId()).execute();
   } else {
       // handle error
   }
}

void Student::reserveBook(Book* book) {
   reservedBooks.push_back(book);
   table.insert("reserved_books").values(id, book->getId()).execute();
}

void Library::addBook(Book& book) {
   table.insert("books").values(book.getId(), book.getTitle(), book.getAuthor(), book.getPublication(), book.isAvailable(), nullptr, nullptr).execute();
}

void Library::removeBook(Book& book) {
   table.remove().from("books").where("id = :id").bind("id", book.getId()).execute();
}

void Library::addStudent(Student& student) {
   table.insert("students").values(student.getId(), student.getName(), student.getEmail(), student.getPhone()).execute();
}

void Library::removeStudent(Student& student) {
   table.remove().from("students").where("id = :id").bind("id", student.getId()).execute();
}

void Library::borrowBook(Student& student, Book& book) {
   book.borrow(&student);
}

void Library::returnBook(Student& student, Book& book) {
   book.returnBook();
}

void Library::reserveBook(Student& student, Book& book) {
   book.reserve(&student);
}

void Library::addFine(Student& student, double fine) {
   table.insert("fines").values(student.getId(), fine).execute();
}

vector<double> Library::viewFines() {
   vector<double> result;
   auto res = table.select("fine").execute();
   for (auto row : res) {
       double fine = row[0];
       result.push_back(fine);
   }
   return result;
}

void Admin::addBook(Library& library, Book& book) {
   library.addBook(book);
}

void Admin::removeBook(Library& library, Book& book) {
   library.removeBook(book);
}

void Admin::removeStudent(Library& library, Student& student) {
   library.removeStudent(student);
}

void Admin::addFine(Library& library, Student& student, double fine) {
   library.addFine(student, fine);
}

vector<double> Admin::viewFines() {
   return table.select("fine").execute();
}

int main() {
   Session session("localhost", 3306, "username", "password", "library");
   Table booksTable = session.getSchema("library").getTable("books");
   Table studentsTable = session.getSchema("library").getTable("students");

   Login login("admin", "admin", session);
   Admin admin(1, "Admin", "admin@admin.com", "123456789", login, studentsTable);
   Library library(session);

   Book book1(1, "The Great Gatsby", "F. Scott Fitzgerald", "Scribner", true, nullptr, nullptr, booksTable);
   Book book2(2, "To Kill a Mockingbird", "Harper Lee", "J. B. Lippincott & Co.", true, nullptr, nullptr, booksTable);
   Book book3(3, "1984", "George Orwell", "Secker & Warburg", true, nullptr, nullptr, booksTable);

   Student student1(1, "John Doe", "john.doe@student.com", "9876543210", login, studentsTable);
   Student student2(2, "Jane Doe", "jane.doe@student.com", "9876543210", login, studentsTable);

   admin.addBook(library, book1);
   admin.addBook(library, book2);
   admin.addBook(library, book3);

   admin.addFine(library, student1, 10.0);

   library.borrowBook(student1, book1);
   library.borrowBook(student2, book2);
   library.reserveBook(student2, book1);
   library.reserveBook(student1, book3);

   library.returnBook(student1, book1);
   library.reserveBook(student2, book1);

   vector<Book*> borrowedBooks = student1.getBorrowedBooks();
   for (auto book : borrowedBooks) {
       cout << book->getTitle() << endl;
   }

   vector<Book*> reservedBooks = student2.getReservedBooks();
   for (auto book : reservedBooks) {
       cout << book->getTitle() << endl;
   }

   vector<double> fines = student1.getFines();
   for (auto fine : fines) {
       cout << fine << endl;
   }

   fines = library.viewFines();
   for (auto fine : fines) {
       cout << fine << endl;
   }

   return 0;
}