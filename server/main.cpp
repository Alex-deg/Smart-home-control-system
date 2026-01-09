#include <iostream>
#include "DataBase.h"

int main(){
  
  DataBase db;
  db.open("Data/smart_home.db");
  db.execute_request("CREATE TABLE users("
                     "id INT PRIMARY KEY, "
                     "name TEXT NOT NULL, "
                     "password TEXT NOT NULL"
                     ")"
  );
  return 0;

}
