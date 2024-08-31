#include <iostream>

class Error {
public:
	virtual void raise() = 0;

protected:
	Error() = default;
	~Error() = default;
	Error(const std::string& m);
	Error(std::string&& m);

	std::string message;
};

class FatalError : public Error {
public:
	FatalError(const std::string& m) : Error(m) {}
	FatalError(std::string&& m) : Error(m) {}
	~FatalError() = default;

	void raise();
};

class WarningError : public Error {
public:
	WarningError(const std::string& m) : Error(m) {}
	WarningError(std::string&& m) : Error(m) {}
	~WarningError() = default;

	void raise();
};
