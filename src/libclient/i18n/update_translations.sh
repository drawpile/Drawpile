for ts in *.ts
do
lupdate ../../libclient ../../desktop/ -recursive -no-obsolete -ts "$ts"
done

