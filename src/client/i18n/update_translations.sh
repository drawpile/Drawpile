for ts in *.ts
do
lupdate .. -recursive -no-obsolete -ts "$ts"
done

