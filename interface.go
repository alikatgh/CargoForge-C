package localinterface

import (
	"fmt"
	"io"
	"os"
)

type error interface {
	Error() string
}

type networkProblem struct {
	message string
	code    int
}

func (np networkProblem) Error() string {
	return fmt.Sprintf("network error! message: %s, code: %v", np.message, np.code)
}

func handleErr(err error) {
	fmt.Println(err.Error())
}

np := networkProblem{
	message: "we received a problem",
	code:    404,
}

handleErr(np)

// prints "network error! message: we received a problem, code: 404"

type File interface {
	io.Closer
	io.Reader
	io.Writer	
	Readdir(count in)([]os.FileInfo, error)
	Slat() (os.FileInfo, error)
}

type car interface {
	Color() string
	Speed() int
}

type firetruck interface {
	car
	HoseLength() int
}
