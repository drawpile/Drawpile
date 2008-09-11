/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include <cassert>
#include <iostream>
#include <cstring>
#include <QByteArray>

#include "message.h"
#include "stroke.h"
#include "binary.h"
#include "login.h"
#include "toolselect.h"

using namespace protocol;
using std::cout;

void hexdump(const char *str, int len) {
	QByteArray ba(str, len);
	cout << ba.toHex().constData() << '\n';
}

Packet *serTest(Packet &pk) {
	QByteArray buf = pk.serialize();
	hexdump(buf, pk.length());
	assert(buf.length() == pk.length());
	Packet *pk2 = Packet::deserialize(buf);
	assert(pk2!=0);
	return pk2;
}

void printMessage(Message *msg) {
	cout << "Deserialized: " << msg->message().toUtf8().constData() << "\n";
	cout << "Tokens:\n";
	foreach(QString tk, msg->tokens())
		cout << "\t-->" << tk.toUtf8().constData() << "<--\n";
}

void printPoints(StrokePoint *sp) {
	cout << "Deserialized: " << sp->user() << ":\t" << sp->point(0).x << ", " << sp->point(0).y << ", " << sp->point(0).z << "\n";
	for(int i=1;i<sp->points();++i)
		cout << "                \t" << sp->point(i).x << ", " << sp->point(i).y << ", " << sp->point(i).z << "\n";
}

void printLogin(LoginId *log) {
	char tmp[5];
	memcpy(tmp, log->magicNumber(), 4);
	tmp[4] = 0;
	cout << "Deserialized: " << tmp << ", " << log->protocolRevision() << ", " << log->softwareVersion() << "\n";
}

void printToolSel(ToolSelect *ts) {
	cout << "Deserialized: " << ts->user() << "," << ts->tool() << "," << ts->mode() << "\n\t"
		<< ts->c1() << ", " << ts->c0() << "; "
		<< ts->s1() << ", " << ts->s0() << "; "
		<< ts->h1() << ", " << ts->h0() << "; "
		<< ts->spacing() << "\n";
}

int main() {
	cout << "Testing message:\n";
	QStringList msglst;
	msglst.append("Terve");
	msglst.append("vaan");
	msglst.append("moi vaan");
	msglst.append("\\\\");
	msglst.append("jee  \"joo\"");
	QString msgstr = Message::quote(msglst);
	std::cout << msgstr.toUtf8().constData() << "\n";
	Message msg(msgstr);
	printMessage((Message*)serTest(msg));

	cout << "\nTesting strokepoint:\n";
	StrokePoint sp(0, 1,100,128);
	printPoints((StrokePoint*)serTest(sp));

	cout << "More:\n";
	sp.addPoint(2, 101, 200);
	sp.addPoint(3, 102, 250);
	printPoints((StrokePoint*)serTest(sp));

	cout << "Testing stroke end:\n";
	StrokeEnd se(12);
	cout << "user: " << ((StrokeEnd*)serTest(se))->user() << "\n";

	cout << "Testing binary\n";
	const char btestptr[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
	QByteArray bintest(btestptr,20);
	BinaryChunk bc(bintest);
	BinaryChunk *deserbin = (BinaryChunk*)serTest(bc);
	hexdump(deserbin->data().constData(), deserbin->payloadLength());

	cout << "Testing login\n";
	LoginId login(MAGIC, REVISION, 10);
	printLogin((LoginId*)serTest(login));

	cout << "Testing tool select\n";
	ToolSelect ts(1,0,10,0xffffffff,0x000000ff, 255, 0, 128, 65, 50);
	printToolSel((ToolSelect*)serTest(ts));

}

