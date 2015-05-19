sqlite modern cpp wrapper
====

This library is a lightweight extensible modern wrapper around sqlite C api .

```c++
#include<iostream>
#include "sqlite_modern_cpp.h"
using namespace  sqlite;
using namespace std;


	try {
		// creates a database file 'dbfile.db' if it does not exists.
		database db("dbfile.db");

		// executes the query and creates a 'user' table
		db <<
			"create table if not exists user ("
			"	_id integer primary key autoincrement not null,"
			"	age int,"
			"	name text,"
			"	weight real"
			");";

		// inserts a new user record.
		// binds the fields to '?' .
		// note that only types allowed for bindings are :
		//		int ,long, long long, float, double
		//		string , u16string
		// sqlite3 only supports utf8 and utf16 strings, you should use std::string for utf8 and std::u16string for utf16.
		// note that u"my text" is a utf16 string literal of type char16_t * .
		db << "insert into user (age,name,weight) values (?,?,?);"
			<< 20
			<< u"bob" // utf16 string
			<< 83.25f;

		db << u"insert into user (age,name,weight) values (?,?,?);" // utf16 query string
			<< 21
			<< "jack"
			<< 68.5;
			
		cout << "The new record got assigned id " << db.last_insert_rowid() << endl;

		// slects from user table on a condition ( age > 18 ) and executes
		// the lambda for each row returned .
		db << "select age,name,weight from user where age > ? ;"
			<< 18
			>> [&](int age, string name, double weight) {
			cout << age << ' ' << name << ' ' << weight << endl;
		};

		// selects the count(*) from user table
		// note that you can extract a single culumn single row result only to : int,long,long,float,double,string,u16string
		int count = 0;
		db << "select count(*) from user" >> count;
		cout << "cout : " << count << endl;

		// this also works and the returned value will be automatically converted to string
		string str_count;
		db << "select count(*) from user" >> str_count;
		cout << "scount : " << str_count << endl;
	}
	catch (exception& e) {
		cout << e.what() << endl;
	}

```

Transactions
=====
You can use transactions with `begin;`, `commit;` and `rollback;` commands.
*(don't forget to put all the semicolons at the end of each query)*.

```c++
		db << "begin;"; // begin a transaction ...   
		db << "insert into user (age,name,weight) values (?,?,?);"
			<< 20
			<< u"bob"
			<< 83.25f;
		db << "insert into user (age,name,weight) values (?,?,?);" // utf16 string
			<< 21
			<< u"jack"
			<< 68.5;
		db << "commit;"; // commit all the changes.
                
		db << "begin;"; // begin another transaction ....
		db << "insert into user (age,name,weight) values (?,?,?);" // utf16 string
			<< 19
			<< u"chirs"
			<< 82.7;
		db << "rollback;"; // cancel this transaction ...

```

Dealing with NULL values
=====
If you have databases where some rows may be null, you can use boost::optional to retain the NULL value between C++ variables and the database. Note that you must enable the boost support by including the type extension for it.

```c++

	#include "sqlite_modern_cpp.h"
	#include "extensions/boost_optional.h"
	
	struct User {
		long long _id;
		boost::optional<int> age;
		boost::optional<string> name;
		boost::optional<real> weight;
	};
	
	{
		User user;
		user.name = "bob";
		
		// Same database as above
		database db("dbfile.db");
		
		// Here, age and weight will be inserted as NULL in the database.
		db << "insert into user (age,name,weight) values (?,?,?);"
			<< user.age
			<< user.name
			<< user.weight;
			
		user._id = db.last_insert_rowid();
	}
	
	{
		// Here, the User instance will retain the NULL value(s) from the database.
		db << "select _id,age,name,weight from user where age > ? ;"
			<< 18
			>> [&](long long id,
				boost::optional<int> age, 
				boost::optional<string> name
				boost::optional<real> weight) {
			
			User user;
			user._id = id;
			user.age = age;
			user.name = move(name);
			user.weight = weight;
			
			cout << "id=" << user._id
				<< " age = " << (user.age ? to_string(*user.age) ? string("NULL"))
				<< " name = " << (user.name ? *user.name : string("NULL"))
				<< " weight = " << (user.weight ? to_string(*user.weight) : string(NULL))
				<< endl;
		};
	}
```

Type extension and serialization
=====
The librarie is extenable for any type you want, an example is the boost::optinal implementation above. But here an example of what it takes to make youre own type:
```c++
// This is the object you wanne save to the DB without a lot of hassel
class Foo {
public:
	int val = 0;
	Foo(int val):val(val) {}
	~Foo() {}
};

// First you need to supply a serialization methode, you can do that in youre own code like so:
namespace sqlite {
	template<> database_binder& operator <<(database_binder& db, const Bam& val) { // here you specialize the operator to take youre object

		db << (int)val.val; // calls the operator<<() for int values so we can save our only member data field
							// for more complex examples see extensions/boost_json_spirit.h there we write a blob to the db
		
		// we could also use mysqlite directly instead of utelising an existing function like this:
		// if(sqlite3_bind_int(db._stmt, db._inx, val) != SQLITE_OK) {
		//		db.throw_sqlite_error();
		// }

		++db._inx; // boilerplate code has to be here dont care about it
		return db;
	}
}

// we can now write our object to the database with ease:
{
	Foo bar1(1);
	db << "insert into test(val) values (?)" << bar1;
}

// to get it back we need to give a deserialization method:
namespace sqlite {
	template<> void get_col_from_db(database_binder& db, int inx, Bam*& ret) {	// again specifie the type it will be used on, a pointer is neccesary if youre type has no default constructor. Otherwise a object ref does the job too.
		auto member = sqlite3_column_int(db._stmt, inx);						// sqlite function to read the column
		ret = new Foo(member);													// create or assign the return value
	}
}

// now we can retrieve our object as we see fit:
{// the simple way
	Bam* b3 = nullptr;
	db << "select val from test" << sqlite::options::throw_on_no_rows >> b3;
	delete b3;
}
{// the more secure way
	std::unique_ptr<Bam> b2;
	db << "select val from test" >> [&](Bam* b) { b2 = std::unique_ptr<Bam>(b); };
}
```

*node: for NDK use the full path to your database file : `sqlite::database db("/data/data/com.your.package/dbfile.db")`*.

##License

MIT license - [http://www.opensource.org/licenses/mit-license.php](http://www.opensource.org/licenses/mit-license.php)
